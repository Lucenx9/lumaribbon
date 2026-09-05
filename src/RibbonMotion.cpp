// SPDX-License-Identifier: GPL-3.0-or-later
#include "RibbonMotion.h"
#include <algorithm>
#include <cmath>

namespace Luma {
RibbonShape RibbonMotion::advance(const Features &f, float seconds) {
    const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.0f, 0.1f) : 0.0f;
    // Non-finite features would poison the springs permanently; hold the shape.
    if (!std::isfinite(f.energy + f.bass + f.mid + f.treble))
        return {value[0], value[1], value[2], value[3]};
    slowEnergy += (f.energy - slowEnergy) * -std::expm1(-dt / 0.85f);
    slowMid += (f.mid - slowMid) * -std::expm1(-dt / 0.65f);
    // Hold the disappearing shape instead of normalizing the noise floor or
    // moving an invisible ribbon home. A resume starts at the held position.
    if (f.energy < 0.002f) {
        velocity.fill(0);
        return {value[0], value[1], value[2], value[3]};
    }
    const float total = std::max(0.08f, f.bass + 1.15f * f.mid + 1.4f * f.treble);
    const float low = f.bass / total;
    const float mid = 1.15f * f.mid / total;
    const float high = 1.4f * f.treble / total;
    const float presence = std::clamp(f.energy / 0.24f, 0.0f, 1.0f);
    const float swell = std::clamp((f.energy - slowEnergy) * 2, -0.5f, 0.5f);
    const float phrase = std::clamp((f.mid - slowMid) * 3, -0.65f, 0.65f);
    const std::array<float, 4> target{
        presence * (0.9f * low - 0.2f * mid - 0.75f * high + 0.24f * swell),
        presence * (0.22f + 0.85f * mid - 0.25f * low - 0.5f * high + 0.3f * phrase),
        presence * (0.65f * (high - low) + 0.35f * phrase),
        std::clamp(0.22f + 0.65f * low + 0.22f * mid + 0.2f * swell, 0.15f, 1.0f)
    };
    constexpr std::array<float, 4> frequency{9, 7, 6, 8};
    for (unsigned i = 0; i < value.size(); ++i) {
        // Exact critically damped step for a held target; retarget from current
        // position and velocity. No timed shape changes or random wandering.
        const float offset = value[i] - target[i];
        const float impulse = velocity[i] + frequency[i] * offset;
        const float decay = std::exp(-frequency[i] * dt);
        value[i] = target[i] + (offset + impulse * dt) * decay;
        velocity[i] = (velocity[i] - frequency[i] * impulse * dt) * decay;
    }
    return {value[0], value[1], value[2], value[3]};
}
}
