// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioEngine.h"
#include "AudioProcessor.h"
#include <chrono>
#include <cmath>
#include <numbers>

namespace Luma {
std::shared_ptr<AudioEngine> AudioEngine::acquire() {
    static std::mutex mutex;
    static std::weak_ptr<AudioEngine> shared;
    std::lock_guard lock(mutex);
    auto engine = shared.lock();
    if (!engine) {
        engine = std::shared_ptr<AudioEngine>(new AudioEngine);
        shared = engine;
    }
    return engine;
}
AudioEngine::AudioEngine() : worker([this](std::stop_token stop) { run(stop); }) {}
AudioEngine::~AudioEngine() {
    worker.request_stop();
    worker.join();
}
AudioSnapshot AudioEngine::snapshot() const {
    std::lock_guard lock(snapshotMutex);
    return latest;
}
void AudioEngine::run(std::stop_token stop) {
    using namespace std::chrono;
    while (!stop.stop_requested()) {
        try {
            AudioRing ring;
            PipeWireCapture capture(ring);
            AudioProcessor processor(ring);
            RibbonMotion motion;
            AudioSnapshot next;
            const auto start = steady_clock::now();
            auto last = start, nextPoll = last;
            bool audible = false;
            while (!stop.stop_requested()) {
                const auto now = steady_clock::now();
                const float dt = std::min(0.1f, duration<float>(now - last).count());
                last = now;
                if (now >= nextPoll) {
                    next.status = capture.poll();
                    nextPoll = now + milliseconds(100);
                }
                const auto features = processor.tick(duration<double>(now.time_since_epoch()).count());
                next.features = features;
                next.shape = motion.advance(features, dt);
                // Phase speed comes from real energy and mids. It freezes after silence.
                const float activity = next.features.energy;
                if (activity > 0.001f) {
                    // Every phase coefficient in both renderers is an integer hundredth.
                    // Their common period avoids float precision loss without a visible jump.
                    next.phase = std::fmod(next.phase + dt * (0.08 + 0.55 * activity + 0.65 * next.features.mid),
                                           200.0 * std::numbers::pi);
                }
                next.time += dt;
                next.dropped = ring.droppedBlocks();
                next.expired = processor.expiredBlocks();
                {
                    std::lock_guard lock(snapshotMutex);
                    latest = next;
                }
                // One shared, coalesced wake-up, never a queued event per frame.
                // The high threshold is visible even at minimum sensitivity.
                if (activity < 0.0005f) audible = false;
                if (!audible && activity >= 0.002f) {
                    audible = true;
                    if (!wakePending.exchange(true)) {
                        QMetaObject::invokeMethod(this, [this] {
                            wakePending.store(false);
                            Q_EMIT audioAvailable();
                        }, Qt::QueuedConnection);
                    }
                }
                std::this_thread::sleep_until(now + milliseconds(10));
            }
        } catch (const std::exception &e) {
            {
                std::lock_guard lock(snapshotMutex);
                latest = {};
                latest.status.error = true;
                latest.status.message = QStringLiteral("Audio analysis unavailable: ") + QString::fromUtf8(e.what()) +
                                        QStringLiteral(". Retrying automatically.");
            }
            // Rare initialization failures retry without retaining a half-built pipeline.
            // Check stop during backoff so removing the final widget stays responsive.
            for (unsigned n = 0; n < 50 && !stop.stop_requested(); ++n)
                std::this_thread::sleep_for(milliseconds(100));
        }
    }
}
} // namespace Luma
