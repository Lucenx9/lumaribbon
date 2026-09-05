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
};

// Worker-owned motion from normalized analyzer features. No Qt, clock or heap
// state; every view receives the same current shape, including late subscribers.
class RibbonMotion {
public:
    RibbonShape advance(const Features &features, float seconds);
private:
    std::array<float, 4> value{0, 0, 0, 0.3f};
    std::array<float, 4> velocity{};
    float slowEnergy = 0;
    float slowMid = 0;
};
}
