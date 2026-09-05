// SPDX-License-Identifier: GPL-3.0-or-later
#include "SignalAnalyzer.h"
#include "AudioIngress.h"
#include "AudioProcessor.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

using namespace Luma;
void check(bool condition, const char *message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void finite(Features f) {
    for (float value : {f.energy, f.bass, f.mid, f.treble, f.onset, f.accents[0], f.accents[1], f.accents[2],
             f.spectralBalance, f.trebleShare})
        check(std::isfinite(value) && value >= 0 && value <= 1, "finite normalized feature");
    check(std::isfinite(f.rms) && std::isfinite(f.peak), "finite raw levels");
}
std::vector<StereoFrame> tone(float hz, float amplitude, unsigned rate, unsigned count, bool antiphase = false, unsigned offset = 0) {
    std::vector<StereoFrame> frames(count);
    for (unsigned i = 0; i < count; ++i) {
        const float sample = amplitude * std::sin(6.283185307179586 * hz * (offset + i) / rate);
        frames[i] = {sample, antiphase ? -sample : sample};
    }
    return frames;
}
void analysisTests() {
    SignalAnalyzer analyzer;
    auto zeros = tone(100, 0, 48000, 48000);
    analyzer.feed(zeros, 48000);
    check(analyzer.features().energy == 0 && analyzer.features().onset == 0, "silence stays dark");
    for (unsigned rate : {8000u, 44100u, 48000u, 96000u, 192000u}) {
        for (float hz : {100.0f, 1000.0f, 3200.0f}) {
            analyzer.reset();
            analyzer.feed(tone(hz, 0.2f, rate, rate * 2), rate);
            const auto f = analyzer.features();
            finite(f);
            check(std::abs(f.rms - 0.141421f) < 0.004f, "RMS sine amplitude");
            check(f.energy > 0.5f, "audible sine produces light");
            const float dominant = hz < 250 ? f.bass : hz < 2500 ? f.mid : f.treble;
            const float others = hz < 250 ? std::max(f.mid, f.treble) : hz < 2500 ? std::max(f.bass, f.treble) : std::max(f.bass, f.mid);
            check(dominant > 5 * others && dominant > 0.4f, "frequency band isolation");
            analyzer.feed(tone(100, 0, rate, rate * 4), rate);
            check(analyzer.features().energy < 0.001f, "decay on digital silence");
        }
    }
    analyzer.reset();
    analyzer.feed(tone(100, 0.2f, 48000, 96000, true), 48000);
    check(analyzer.features().bass > 0.5f, "anti-phase stereo does not cancel");
    analyzer.decay(5);
    check(analyzer.features().energy < 0.001f, "decay on suspended input");
    analyzer.reset();
    analyzer.feed(tone(1000, 0.0002f, 48000, 48000 * 20), 48000);
    check(analyzer.features().energy == 0, "normalization never amplifies sub-gate noise");
    analyzer.reset();
    std::vector<StereoFrame> impulses(48000);
    for (unsigned i = 4096; i < impulses.size(); i += 8192) impulses[i] = {1, 1};
    float onset = 0;
    for (size_t i = 0; i < impulses.size(); i += 128) {
        analyzer.feed(std::span(impulses.data() + i, std::min(size_t(128), impulses.size() - i)), 48000);
        finite(analyzer.features());
        onset = std::max(onset, analyzer.features().onset);
    }
    check(onset > 0.1f, "impulses trigger attack envelope");
    analyzer.reset();
    std::vector<StereoFrame> bad(8192, {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()});
    analyzer.feed(bad, 48000);
    finite(analyzer.features());
    check(analyzer.features().energy == 0, "nonfinite input is silence");
    analyzer.reset();
    analyzer.feed(std::vector<StereoFrame>(8192, {1, 1}), 48000);
    check(analyzer.features().energy == 0, "DC offset is rejected");
    std::cout << "PASS silence, 5 rates × 3 bands, RMS, anti-phase, gate, attacks, finite values, DC, decay\n";
}
void pipelineTests() {
    for (unsigned rate : {8000u, 44100u, 48000u, 192000u}) {
        for (unsigned quantum : {64u, 128u, 256u, 512u, 1024u, 2048u, 4096u}) {
            AudioRing ring;
            AudioIngress ingress(ring);
            AudioProcessor processor(ring);
            unsigned produced = 0;
            Features f;
            for (unsigned ms = 10; ms <= 3000; ms += 10) {
                while (produced + quantum <= uint64_t(rate) * ms / 1000) {
                    auto frames = tone(100, 0.2f, rate, quantum, false, produced);
                    ingress.write(reinterpret_cast<const char *>(frames.data()), quantum, sizeof(StereoFrame), rate, 1, ms / 1000.0);
                    produced += quantum;
                }
                f = processor.tick(ms / 1000.0);
                finite(f);
            }
            if (f.energy < 0.3f) std::cerr << "rate=" << rate << " quantum=" << quantum << '\n';
            check(f.energy > 0.3f, "pipeline works independently of graph quantum");
            for (unsigned ms = 3010; ms <= 9000; ms += 10) f = processor.tick(ms / 1000.0);
            check(f.energy < 0.001f, "pipeline fades after disconnect");
        }
    }
    AudioRing ring;
    AudioIngress ingress(ring);
    AudioProcessor processor(ring);
    auto frames = tone(100, 0.2f, 48000, 48000);
    for (unsigned i = 0; i < 48000; i += 480)
        ingress.write(reinterpret_cast<const char *>(frames.data() + i), 480, sizeof(StereoFrame), 48000, 1, 0);
    check(ring.droppedBlocks() > 0, "stalled consumer has a bounded queue");
    finite(processor.tick(1));
    for (unsigned ms = 1010; ms <= 3000; ms += 10) {
        ingress.write(reinterpret_cast<const char *>(frames.data()), 480, sizeof(StereoFrame), 48000, 2, ms / 1000.0);
        finite(processor.tick(ms / 1000.0));
    }
    check(processor.tick(3.01).energy > 0.3f, "recovery after overflow and new capture generation");
    AudioRing resumedRing;
    AudioIngress resumedIngress(resumedRing);
    AudioProcessor resumedProcessor(resumedRing);
    auto partial = tone(100, 0.2f, 48000, 256);
    resumedIngress.write(reinterpret_cast<const char *>(partial.data()), 256, sizeof(StereoFrame), 48000, 1, 0);
    for (int tick = 1; tick <= 500; ++tick) resumedProcessor.tick(tick * 0.01);
    partial.assign(256, {});
    for (int tick = 501; tick <= 560; ++tick) {
        resumedIngress.write(reinterpret_cast<const char *>(partial.data()), 256, sizeof(StereoFrame), 48000, 1, tick * 0.01);
        const auto resumed = resumedProcessor.tick(tick * 0.01);
        check(resumed.energy == 0 && resumed.onset == 0, "partial block expires across suspension without a format change");
    }
    std::cout << "PASS 28 rate/quantum combinations, disconnect, bounded overflow, recovery\n";
}
void concurrentRingTest() {
    AudioRing ring;
    std::atomic<bool> done{false};
    std::atomic<unsigned> accepted{0};
    std::thread producer([&] {
        AudioBlock b;
        b.count = 512;
        for (uint64_t n = 0; n < 100000; ++n) {
            b.sequence = n;
            b.samples.front().left = b.samples.back().right = float(n);
            if (ring.push(b)) ++accepted;
        }
        done.store(true, std::memory_order_release);
    });
    unsigned consumed = 0;
    uint64_t previous = 0;
    AudioBlock b;
    while (true) {
        if (ring.pop(b)) {
            check(!consumed || b.sequence > previous, "SPSC monotonic sequence");
            check(b.samples.front().left == b.sequence && b.samples.back().right == b.sequence, "SPSC no torn block");
            previous = b.sequence;
            ++consumed;
        } else if (done.load(std::memory_order_acquire)) {
            if (!ring.pop(b)) break;
            ++consumed;
        }
    }
    producer.join();
    check(consumed == accepted, "every accepted block is consumed");
    check(consumed + ring.droppedBlocks() == 100000, "drops are accounted for");
    std::cout << "PASS 100000-block concurrent SPSC stress\n";
}
void staleAudioTest() {
    for (unsigned rate : {8000u, 48000u, 192000u}) {
        AudioRing ring;
        AudioIngress ingress(ring);
        AudioProcessor processor(ring);
        const auto frames = tone(100, 0.2f, rate, 8192);
        // A full queue survives a blocked worker, followed by a long pause.
        ingress.write(reinterpret_cast<const char *>(frames.data()), frames.size(), sizeof(StereoFrame), rate, 1, 10);
        const auto stale = processor.tick(15);
        check(stale.energy == 0 && stale.onset == 0, "queued audio expires during a worker stall");
        check(processor.expiredBlocks() == AudioRing::Capacity, "expired blocks are counted separately from queue overflow");
        // Fresh silence must not combine with pre-stall FFT history.
        std::array<StereoFrame, 512> silence{};
        for (unsigned n = 1; n <= 16; ++n) {
            const double time = 15 + double(n * silence.size()) / rate;
            ingress.write(reinterpret_cast<const char *>(silence.data()), silence.size(), sizeof(StereoFrame), rate, 1, time);
            const auto f = processor.tick(time);
            check(f.energy == 0 && f.onset == 0, "stale audio cannot flash on silent recovery");
        }
        for (unsigned n = 1; n <= 100; ++n) {
            const double time = 20 + double(n * 512) / rate;
            ingress.write(reinterpret_cast<const char *>(frames.data()), 512, sizeof(StereoFrame), rate, 1, time);
            finite(processor.tick(time));
        }
        check(processor.tick(20 + 51200.0 / rate).energy > 0.3f, "fresh audio recovers after expired blocks");
        ingress.write(reinterpret_cast<const char *>(silence.data()), silence.size(), sizeof(StereoFrame), rate, 1, 35);
        const auto resumed = processor.tick(35);
        check(resumed.energy < 0.001f && resumed.onset == 0, "a stalled worker decays its old envelope before fresh silence");
    }
    std::cout << "PASS stale full queues, silent recovery and fresh audio at three rates\n";
}
int main() { analysisTests(); pipelineTests(); concurrentRingTest(); staleAudioTest(); }
