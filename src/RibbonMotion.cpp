// SPDX-License-Identifier: GPL-3.0-or-later
#include "RibbonMotion.h"
#include <algorithm>
#include <cmath>

namespace Luma {
RibbonShape RibbonMotion::advance(const Features &f, float seconds) {
    const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.0f, 0.1f) : 0.0f;
    // Non-finite features would poison the springs permanently; hold the shape.
    if (dt == 0 || !std::isfinite(f.energy) || !std::isfinite(f.bass) || !std::isfinite(f.mid)
        || !std::isfinite(f.treble) || !std::isfinite(f.rms) || !std::isfinite(f.spectralBalance))
        return {value[0], value[1], value[2], value[3], value[4], value[5]};
    slowEnergy += (f.energy - slowEnergy) * -std::expm1(-dt / 0.85f);
    slowMid += (f.mid - slowMid) * -std::expm1(-dt / 0.65f);
    // Hold the disappearing shape instead of normalizing the noise floor or
    // moving an invisible ribbon home. A resume starts at the held position.
    if (f.energy < 0.002f) {
        velocity.fill(0);
        phraseReady = false;
        return {value[0], value[1], value[2], value[3], value[4], value[5]};
    }
    const float total = std::max(0.08f, f.bass + 1.15f * f.mid + 1.4f * f.treble);
    const float low = f.bass / total;
    const float mid = 1.15f * f.mid / total;
    const float high = 1.4f * f.treble / total;
    const float presence = std::clamp(f.energy / 0.24f, 0.0f, 1.0f);
    const float swell = std::clamp((f.energy - slowEnergy) * 2, -0.5f, 0.5f);
    const float phrase = std::clamp((f.mid - slowMid) * 3, -0.65f, 0.65f);
    // Opposing bands can cancel the arch in a full mix. Let their coexistence
    // open a counter-bend instead: zero for one band, strongest in a balanced mix.
    const float blend = 3.0f * (low * mid + mid * high + high * low);
    // Use pre-normalization loudness and timbre so automatic gain adaptation
    // cannot invent a slow crescendo on a stationary tone. Log compression has
    // a fixed scale: quiet input cannot acquire an unbounded relative gain.
    // Initialize from audible input so a pause does not create a false crescendo.
    const std::array<float, 2> phraseLevel{
        std::log1p(12.0f * std::clamp(f.rms, 0.0f, 1.0f)) / std::log(13.0f),
        2.0f * std::clamp(f.spectralBalance, 0.0f, 1.0f) - 1.0f
    };
    if (!phraseReady) {
        phraseReference = phraseLevel;
        phraseReady = true;
    }
    for (unsigned i = 0; i < phraseReference.size(); ++i)
        phraseReference[i] += (phraseLevel[i] - phraseReference[i]) * -std::expm1(-dt / 2.5f);
    const float lift = presence * std::clamp(5.0f * (phraseLevel[0] - phraseReference[0]), -1.0f, 1.0f);
    const float lean = presence * std::clamp(3.5f * (phraseLevel[1] - phraseReference[1]), -1.0f, 1.0f);
    const std::array<float, 6> target{
        presence * (0.9f * low - 0.2f * mid - 0.75f * high + 0.24f * swell),
        presence * (0.22f + 0.85f * mid - 0.25f * low - 0.5f * high + 0.8f * blend + 0.3f * phrase),
        presence * (0.65f * (high - low) + 0.35f * phrase),
        std::clamp(0.38f + 0.5f * low + 0.24f * mid + 0.14f * blend + 0.2f * swell, 0.15f, 1.0f),
        lift, lean
    };
    // Base motion takes about 1.5–2 seconds to follow a held target. Short attacks
    // remain in the separate accent path, not in the broad displacement.
    constexpr std::array<float, 6> frequency{9, 7, 6, 8, 2.4f, 2.0f};
    for (unsigned i = 0; i < value.size(); ++i) {
        // Exact critically damped step for a held target; retarget from current
        // position and velocity. No timed shape changes or random wandering.
        const float offset = value[i] - target[i];
        const float impulse = velocity[i] + frequency[i] * offset;
        const float decay = std::exp(-frequency[i] * dt);
        value[i] = target[i] + (offset + impulse * dt) * decay;
        velocity[i] = (velocity[i] - frequency[i] * impulse * dt) * decay;
    }
    return {value[0], value[1], value[2], value[3], value[4], value[5]};
}
}
