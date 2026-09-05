// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>

namespace Luma {
struct StereoFrame { float left = 0; float right = 0; };
struct AudioBlock {
    static constexpr unsigned Capacity = 512;
    std::array<StereoFrame, Capacity> samples{};
    uint32_t count = 0;
    uint32_t rate = 48000;
    uint32_t generation = 0;
    uint32_t quantumFrames = 512;
    uint64_t sequence = 0;
    double receivedAt = 0; // Monotonic callback receipt time, not speaker presentation time.
};
struct Features {
    float energy = 0;
    float bass = 0;
    float mid = 0;
    float treble = 0;
    // Slow, gated timbre descriptors shared by every view, independent of sensitivity.
    float spectralBalance = 0.5f; // Bass 0, mids 0.65, treble 1.
    float trebleShare = 0;
    float onset = 0;
    std::array<float, 3> accents{}; // Short bass, mid and treble attack envelopes.
    float rippleAge = 10;
    float rippleOrigin = 0.46f;
    float rms = 0;
    float peak = 0;
};
}
