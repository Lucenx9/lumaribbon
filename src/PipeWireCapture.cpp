// SPDX-License-Identifier: GPL-3.0-or-later
#include "PipeWireCapture.h"
#include "AudioIngress.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>
#include <spa/param/audio/format-utils.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>

namespace Luma {
using Clock = std::chrono::steady_clock;
struct PipeWireCapture::Impl {
    struct Sink { QString name, description, serial; };
    AudioRing &ring;
    AudioIngress ingress;
    pw_thread_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    pw_registry *registry = nullptr;
    pw_metadata *metadata = nullptr;
    pw_stream *stream = nullptr;
    spa_hook coreListener{}, registryListener{}, metadataListener{}, streamListener{};
    std::map<uint32_t, Sink> sinks;
    uint32_t metadataId = PW_ID_ANY;
    QString defaultSink, connectedSerial;
    bool disconnected = false, streamFailed = false, badFormat = false;
    pw_stream_state streamState = PW_STREAM_STATE_UNCONNECTED;
    // Param negotiation is on the control loop, process on the RT thread.
    std::atomic<uint32_t> format{0}; // rate << 4 | channels
    std::atomic<uint32_t> generation{0};
    bool retryError = false;
    bool defaultReceived = false;
    Clock::time_point retryAt{};
    Clock::time_point metadataWaitUntil{};
    CaptureStatus status;

    explicit Impl(AudioRing &value) : ring(value), ingress(value) {
        static std::once_flag initialized;
        std::call_once(initialized, [] { pw_init(nullptr, nullptr); });
    }
    ~Impl() { close(); }

    static void coreError(void *data, uint32_t id, int, int res, const char *) {
        auto &self = *static_cast<Impl *>(data);
        if (id == PW_ID_CORE && res < 0) self.disconnected = true;
    }
    static int property(void *data, uint32_t subject, const char *key, const char *, const char *value) {
        auto &self = *static_cast<Impl *>(data);
        if (subject != PW_ID_CORE) return 0;
        if (!key || std::strcmp(key, "default.audio.sink") == 0) {
            self.defaultSink = value ? QJsonDocument::fromJson(QByteArray(value)).object().value("name").toString() : QString();
            self.defaultReceived = true;
        }
        return 0;
    }
    static void global(void *data, uint32_t id, uint32_t, const char *type, uint32_t version, const spa_dict *props) {
        auto &self = *static_cast<Impl *>(data);
        if (!props) return;
        const auto value = [props](const char *key) {
            const char *text = spa_dict_lookup(props, key);
            return text ? QString::fromUtf8(text) : QString();
        };
        if (std::strcmp(type, PW_TYPE_INTERFACE_Node) == 0 && value(PW_KEY_MEDIA_CLASS) == "Audio/Sink") {
            if (self.sinks.size() < 128)
                self.sinks[id] = {value(PW_KEY_NODE_NAME), value(PW_KEY_NODE_DESCRIPTION), value(PW_KEY_OBJECT_SERIAL)};
        } else if (std::strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0 && value(PW_KEY_METADATA_NAME) == "default" && !self.metadata) {
            self.metadata = static_cast<pw_metadata *>(pw_registry_bind(self.registry, id, type, std::min(version, uint32_t(PW_VERSION_METADATA)), 0));
            if (self.metadata) {
                self.metadataId = id;
                static const pw_metadata_events events = [] { pw_metadata_events e{}; e.version = PW_VERSION_METADATA_EVENTS; e.property = property; return e; }();
                pw_metadata_add_listener(self.metadata, &self.metadataListener, &events, &self);
            }
        }
    }
    static void removed(void *data, uint32_t id) {
        auto &self = *static_cast<Impl *>(data);
        self.sinks.erase(id);
        if (id == self.metadataId && self.metadata) {
            spa_hook_remove(&self.metadataListener);
            pw_proxy_destroy(reinterpret_cast<pw_proxy *>(self.metadata));
            self.metadata = nullptr;
            self.metadataId = PW_ID_ANY;
            self.defaultSink.clear();
            self.defaultReceived = false;
            self.metadataWaitUntil = Clock::now() + std::chrono::seconds(1);
        }
    }
    static void stateChanged(void *data, pw_stream_state, pw_stream_state state, const char *) {
        auto &self = *static_cast<Impl *>(data);
        self.streamState = state;
        if (state != PW_STREAM_STATE_STREAMING) self.generation.fetch_add(1, std::memory_order_relaxed);
        if (state == PW_STREAM_STATE_ERROR || state == PW_STREAM_STATE_UNCONNECTED) self.streamFailed = true;
    }
    static void paramChanged(void *data, uint32_t id, const spa_pod *param) {
        auto &self = *static_cast<Impl *>(data);
        if (id != SPA_PARAM_Format) return;
        self.format.store(0, std::memory_order_release);
        self.generation.fetch_add(1, std::memory_order_relaxed);
        if (!param) return;
        spa_audio_info_raw info{};
        self.badFormat = spa_format_audio_raw_parse(param, &info) < 0
            || info.format != SPA_AUDIO_FORMAT_F32 || info.channels != 2 || info.rate < 8000 || info.rate > 192000;
        if (!self.badFormat) self.format.store((info.rate << 4) | info.channels, std::memory_order_release);
    }
    static void process(void *data) noexcept {
        auto &self = *static_cast<Impl *>(data);
        pw_buffer *buffer = pw_stream_dequeue_buffer(self.stream);
        if (!buffer) return;
        const auto negotiated = self.format.load(std::memory_order_acquire);
        const auto *buf = buffer->buffer;
        if (negotiated && buf && buf->n_datas == 1) {
            const auto &plane = buf->datas[0];
            if (plane.data && plane.chunk && plane.maxsize && !(plane.chunk->flags & SPA_CHUNK_FLAG_CORRUPTED)) {
                const uint32_t channels = negotiated & 15;
                const uint32_t packed = channels * sizeof(float);
                const int32_t stride = plane.chunk->stride == 0 ? int32_t(packed) : plane.chunk->stride;
                const uint32_t offset = plane.chunk->offset % plane.maxsize;
                const uint32_t bytes = std::min(plane.chunk->size, plane.maxsize - offset);
                if (stride >= int32_t(packed) && bytes >= packed) {
                    const uint32_t frames = 1 + (bytes - packed) / uint32_t(stride);
                    const auto *base = static_cast<const char *>(plane.data) + offset;
                    self.ingress.write(base, frames, uint32_t(stride), negotiated >> 4,
                        self.generation.load(std::memory_order_relaxed),
                        std::chrono::duration<double>(Clock::now().time_since_epoch()).count());
                }
            }
        }
        pw_stream_queue_buffer(self.stream, buffer);
    }
    void destroyStream() {
        if (stream) {
            // Older supported PipeWire releases do not synchronize hook removal.
            // Disconnect first to stop the data node and all in-flight processing.
            pw_stream_disconnect(stream);
            spa_hook_remove(&streamListener);
            pw_stream_destroy(stream);
            stream = nullptr;
        }
        format.store(0, std::memory_order_release);
        generation.fetch_add(1, std::memory_order_relaxed);
        ring.keepNewest(0);
        connectedSerial.clear();
        streamFailed = badFormat = false;
        streamState = PW_STREAM_STATE_UNCONNECTED;
    }
    void close() {
        if (!loop) return;
        pw_thread_loop_stop(loop);
        destroyStream();
        if (metadata) { spa_hook_remove(&metadataListener); pw_proxy_destroy(reinterpret_cast<pw_proxy *>(metadata)); metadata = nullptr; }
        if (registry) { spa_hook_remove(&registryListener); pw_proxy_destroy(reinterpret_cast<pw_proxy *>(registry)); registry = nullptr; }
        if (core) { spa_hook_remove(&coreListener); pw_core_disconnect(core); core = nullptr; }
        if (context) { pw_context_destroy(context); context = nullptr; }
        pw_thread_loop_destroy(loop);
        loop = nullptr;
        sinks.clear();
        defaultSink.clear();
        metadataId = PW_ID_ANY;
    }
    bool connect() {
        disconnected = false;
        defaultReceived = false;
        metadataWaitUntil = Clock::now() + std::chrono::seconds(1);
        loop = pw_thread_loop_new("luma-capture", nullptr);
        if (!loop) return false;
        context = pw_context_new(pw_thread_loop_get_loop(loop), nullptr, 0);
        if (!context) { close(); return false; }
        core = pw_context_connect(context, pw_properties_new(PW_KEY_APP_NAME, "Luma Ribbon", nullptr), 0);
        if (!core) { close(); return false; }
        static const pw_core_events coreEvents = [] { pw_core_events e{}; e.version = PW_VERSION_CORE_EVENTS; e.error = coreError; return e; }();
        pw_core_add_listener(core, &coreListener, &coreEvents, this);
        registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
        if (!registry) { close(); return false; }
        static const pw_registry_events registryEvents = [] { pw_registry_events e{}; e.version = PW_VERSION_REGISTRY_EVENTS; e.global = global; e.global_remove = removed; return e; }();
        pw_registry_add_listener(registry, &registryListener, &registryEvents, this);
        if (pw_thread_loop_start(loop) < 0) { close(); return false; }
        return true;
    }
    void createStream(const Sink &sink) {
        retryError = false;
        const auto serial = sink.serial.toUtf8();
        stream = pw_stream_new(core, "Luma Ribbon · monitor", pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "Music",
            PW_KEY_APP_NAME, "Luma Ribbon", PW_KEY_NODE_NAME, "luma-ribbon-monitor",
            PW_KEY_STREAM_CAPTURE_SINK, "true", PW_KEY_TARGET_OBJECT, serial.constData(),
            "node.dont-fallback", "true", PW_KEY_NODE_DONT_RECONNECT, "true",
            "node.dont-move", "true", PW_KEY_NODE_PASSIVE, "true", nullptr));
        if (!stream) { streamFailed = true; return; }
        static const pw_stream_events events = [] {
            pw_stream_events e{}; e.version = PW_VERSION_STREAM_EVENTS;
            e.state_changed = stateChanged; e.param_changed = paramChanged; e.process = process; return e;
        }();
        pw_stream_add_listener(stream, &streamListener, &events, this);
        uint8_t storage[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(storage, sizeof(storage));
        // PipeWire converts the sink's channel layout/encoding to interleaved stereo.
        // Rate remains negotiated, and is read from SPA_PARAM_Format for each epoch.
        spa_audio_info_raw info{};
        info.format = SPA_AUDIO_FORMAT_F32;
        info.channels = 2;
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;
        const spa_pod *params[] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};
        const auto flags = pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS);
        connectedSerial = sink.serial;
        if (pw_stream_connect(stream, PW_DIRECTION_INPUT, PW_ID_ANY, flags, params, 1) < 0) streamFailed = true;
    }
    CaptureStatus poll() {
        const auto now = Clock::now();
        if (!loop) {
            if (now < retryAt) return status;
            if (!connect()) {
                retryAt = now + std::chrono::seconds(2);
                return status = {QStringLiteral("PipeWire is unavailable. Reconnecting automatically."), {}, true};
            }
        }
        pw_thread_loop_lock(loop);
        struct UnlockOnExit {
            pw_thread_loop *value;
            ~UnlockOnExit() { if (value) pw_thread_loop_unlock(value); }
        } unlock{loop};
        if (disconnected) {
            pw_thread_loop_unlock(loop);
            unlock.value = nullptr;
            close();
            retryAt = now + std::chrono::seconds(1);
            return status = {QStringLiteral("The PipeWire connection was interrupted. Reconnecting automatically."), {}, true};
        }
        const Sink *target = nullptr;
        for (const auto &[id, sink] : sinks) {
            if (!defaultSink.isEmpty() && sink.name == defaultSink && !sink.serial.isEmpty()) { target = &sink; break; }
        }
        if (!target) {
            const bool awaitingMetadata = !defaultReceived && now < metadataWaitUntil;
            const bool previousSinkExists = std::any_of(sinks.begin(), sinks.end(), [this](const auto &entry) {
                return entry.second.serial == connectedSerial;
            });
            if (!awaitingMetadata || !previousSinkExists || streamFailed) destroyStream();
            status = {awaitingMetadata ? QStringLiteral("Waiting for audio routing information.")
                : QStringLiteral("No default audio output is available. Select an output in Plasma's audio settings."), {}, !awaitingMetadata};
        } else {
            if (streamFailed || connectedSerial != target->serial) {
                const bool failed = streamFailed;
                destroyStream();
                if (failed) { retryAt = now + std::chrono::seconds(1); retryError = true; }
            }
            if (!stream && now >= retryAt) createStream(*target);
            const auto f = format.load(std::memory_order_acquire);
            const bool error = badFormat || streamFailed || retryError;
            status = {badFormat ? QStringLiteral("Unsupported audio format. Stereo float PCM at 8–192 kHz is required.")
                : error ? QStringLiteral("Cannot read the audio monitor. Check PipeWire and WirePlumber. The connection will be retried.")
                : streamState == PW_STREAM_STATE_STREAMING ? QStringLiteral("Audio monitor connected.")
                : QStringLiteral("Waiting for audio on the default output."), target->description, error, f >> 4, f & 15};
        }
        return status;
    }
};
PipeWireCapture::PipeWireCapture(AudioRing &ring) : impl(std::make_unique<Impl>(ring)) {}
PipeWireCapture::~PipeWireCapture() = default;
CaptureStatus PipeWireCapture::poll() { return impl->poll(); }
}
