// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AudioRing.h"
#include <algorithm>
#include <cstring>

namespace Luma {
// Called only by the producer, with validated interleaved stereo float data.
// Accumulation makes queue capacity independent of PipeWire's graph quantum.
class AudioIngress {
public:
    explicit AudioIngress(AudioRing &destination) : ring(destination) {}
    void write(const char *data, uint32_t frames, uint32_t stride, uint32_t rate, uint32_t generation, double monotonicTime) noexcept {
        const double maximumGap = std::max(0.12, 2.0 * std::max(frames, previousQuantum) / rate);
        if (block.rate != rate || block.generation != generation || frames > 8192
            || (lastWrite >= 0 && monotonicTime - lastWrite > maximumGap)) block.count = 0;
        lastWrite = monotonicTime;
        previousQuantum = frames;
        block.rate = rate;
        block.generation = generation;
        block.quantumFrames = frames;
        for (uint32_t i = frames > 8192 ? frames - 8192 : 0; i < frames; ++i) {
            auto &sample = block.samples[block.count++];
            std::memcpy(&sample.left, data + size_t(i) * stride, sizeof(float));
            std::memcpy(&sample.right, data + size_t(i) * stride + sizeof(float), sizeof(float));
            if (block.count == AudioBlock::Capacity) {
                block.sequence = sequence++;
                block.receivedAt = monotonicTime;
                ring.push(block);
                block.count = 0;
            }
        }
    }
private:
    AudioRing &ring;
    AudioBlock block;
    uint64_t sequence = 0;
    double lastWrite = -1;
    uint32_t previousQuantum = 0;
};
}
