// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AudioTypes.h"
#include <array>

namespace Luma {
struct RibbonShape {
    float arch = 0;
    float counterBend = 0;
    float bias = 0;
    float opening = 0.3f;
    float lift = 0; // Slow loudness trend.
    float lean = 0; // Slow timbre trend.
};

// Worker-owned motion from analyzer features. No Qt, clock or heap
// state; every view receives the same current shape, including late subscribers.
class RibbonMotion {
public:
    RibbonShape advance(const Features &features, float seconds);
private:
    std::array<float, 6> value{0, 0, 0, 0.3f, 0, 0};
    std::array<float, 6> velocity{};
    float slowEnergy = 0;
    float slowMid = 0;
    std::array<float, 2> phraseReference{};
    bool phraseReady = false;
};
}
