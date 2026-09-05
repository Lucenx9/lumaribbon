// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AudioTypes.h"
#include <fftw3.h>
#include <span>

namespace Luma {
class SignalAnalyzer {
public:
    static constexpr unsigned Window = 2048;
    static constexpr unsigned Hop = 512;
    SignalAnalyzer();
    ~SignalAnalyzer();
    SignalAnalyzer(const SignalAnalyzer &) = delete;
    SignalAnalyzer &operator=(const SignalAnalyzer &) = delete;
    void feed(std::span<const StereoFrame> frames, uint32_t sampleRate);
    void decay(float seconds);
    void reset();
    void discontinuity();
    Features features() const { return result; }
private:
    static constexpr unsigned MaxWindow = 8192;
    void prepareWindow(unsigned size);
    void analyze();
    std::array<StereoFrame, MaxWindow> history{};
    std::array<float, MaxWindow> window{};
    std::array<float, MaxWindow> input{};
    std::array<std::array<float, 2>, MaxWindow / 2 + 1> output{};
    std::array<float, MaxWindow / 2 + 1> powers{};
    std::array<float, MaxWindow / 2 + 1> previousMagnitudes{};
    std::array<float, 3> fluxMean{}, bandReference{}, accentPeak{}, cooldown{};
    bool spectrumReady = false;
    fftwf_plan plan = nullptr;
    unsigned windowSize = Window, hopSize = Hop;
    unsigned cursor = 0, filled = 0, sinceHop = 0;
    uint32_t rate = 48000;
    float reference = 0.12f;
    float onsetCooldown = 0;
    Features result;
};
}
