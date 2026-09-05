// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// PROTOTYPE: can audible changes reshape the ribbon without a travelling-wave loop?
#include "AudioTypes.h"
#include <algorithm>
#include <array>
#include <cmath>

struct MacroMotion {
    // Arch, counter-bend, horizontal bias, filament opening. Shared by every view.
    std::array<float, 4> value{0, 0, 0, 0.3f};
    std::array<float, 4> velocity{};
    float slowEnergy = 0;
    float slowMid = 0;

    void advance(const Luma::Features &f, float dt) {
        dt = std::clamp(dt, 0.0f, 0.1f);
        slowEnergy += (f.energy - slowEnergy) * -std::expm1(-dt / 0.85f);
        slowMid += (f.mid - slowMid) * -std::expm1(-dt / 0.65f);
        // Do not normalize the noise floor or move an invisible ribbon back home.
        if (f.energy < 0.002f) {
            velocity.fill(0);
            return;
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
        const std::array<float, 4> frequency{9, 7, 6, 8};
        for (unsigned i = 0; i < value.size(); ++i) {
            // Exact critically damped spring step for a held target. Changes start
            // from the current shape and velocity, with no random or timed targets.
            const float offset = value[i] - target[i];
            const float impulse = velocity[i] + frequency[i] * offset;
            const float decay = std::exp(-frequency[i] * dt);
            value[i] = target[i] + (offset + impulse * dt) * decay;
            velocity[i] = (velocity[i] - frequency[i] * impulse * dt) * decay;
        }
    }
};
