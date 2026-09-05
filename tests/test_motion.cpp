// SPDX-License-Identifier: GPL-3.0-or-later
#include "RibbonMotion.h"
#include "SignalAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>

using namespace Luma;
namespace {
void check(bool condition, const char *message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::array<float, 4> values(RibbonShape s) { return {s.arch, s.counterBend, s.bias, s.opening}; }
float distance(RibbonShape a, RibbonShape b) {
    float result = 0;
    for (unsigned i = 0; i < 4; ++i) result = std::max(result, std::abs(values(a)[i] - values(b)[i]));
    return result;
}
void finite(RibbonShape s) {
    for (float v : values(s)) check(std::isfinite(v) && std::abs(v) < 1.5f, "finite bounded shape");
    check(s.opening >= 0.14f && s.opening <= 1.01f, "bounded filament opening");
}
}

int main() {
    RibbonMotion silent;
    const auto initial = silent.advance({}, 0.01f);
    for (unsigned i = 0; i < 2000; ++i)
        check(distance(initial, silent.advance({}, 0.01f)) == 0, "silence never invents movement");

    // Cross the production analyzer interface, not hand-tuned display features.
    SignalAnalyzer analyzer;
    RibbonMotion motion;
    std::array<RibbonShape, 3> settled;
    std::array<StereoFrame, 480> block;
    uint64_t index = 0;
    RibbonShape previous;
    float maxStep = 0;
    const std::array<float, 3> frequencies{100, 1000, 6000};
    for (unsigned band = 0; band < frequencies.size(); ++band) {
        RibbonShape nearEnd;
        for (unsigned tick = 0; tick < 800; ++tick) {
            for (auto &frame : block) {
                const float sample = 0.2f * std::sin(2 * std::numbers::pi * frequencies[band] * index++ / 48000);
                frame = {sample, -sample}; // Stereo power must survive phase cancellation.
            }
            analyzer.feed(block, 48000);
            const auto current = motion.advance(analyzer.features(), 0.01f);
            finite(current);
            maxStep = std::max(maxStep, distance(current, previous));
            previous = current;
            if (tick == 699) nearEnd = current;
        }
        settled[band] = previous;
        check(distance(previous, nearEnd) < 0.003f, "a sustained tone settles instead of looping shapes");
    }
    check(settled[0].arch > 0.65f && settled[0].bias < -0.4f, "bass produces a broad arch");
    check(settled[1].counterBend > 0.8f, "mids produce a distinct counter-bend");
    check(settled[2].arch < -0.55f && settled[2].bias > 0.4f, "highs produce an opposite shallow arch");
    check(maxStep < 0.055f, "abrupt band switches stay continuous at worker cadence");

    // Missing input decays through the real analyzer; the last invisible shape holds.
    for (unsigned i = 0; i < 700; ++i) {
        analyzer.decay(0.01);
        previous = motion.advance(analyzer.features(), 0.01f);
        finite(previous);
    }
    check(analyzer.features().energy < 0.001f, "decays fully to silence");
    const auto held = previous;
    Features noise;
    noise.energy = 0.001f;
    noise.mid = 1;
    for (unsigned i = 0; i < 1000; ++i)
        check(distance(held, motion.advance(noise, 0.01f)) == 0, "sub-gate input cannot steer the held shape");

    Features loud;
    loud.energy = loud.bass = 1;
    check(distance(held, motion.advance(loud, 0.01f)) < 0.015f, "resume starts from the held position");
    previous = motion.advance(loud, 0.01f);
    for (float dt : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
        check(distance(previous, motion.advance(loud, dt)) < 0.000001f, "invalid elapsed time cannot advance motion");

    Features poisoned;
    poisoned.energy = poisoned.mid = std::numeric_limits<float>::quiet_NaN();
    poisoned.bass = -std::numeric_limits<float>::infinity();
    poisoned.treble = 1;
    for (unsigned i = 0; i < 10; ++i)
        check(distance(previous, motion.advance(poisoned, 0.01f)) == 0, "non-finite features cannot corrupt motion state");
    const RibbonShape recovered = motion.advance(loud, 0.01f);
    finite(recovered);
    check(distance(previous, recovered) > 0.001f, "motion keeps responding after non-finite features");

    RibbonMotion stalled, capped;
    check(distance(stalled.advance(loud, 10), capped.advance(loud, 0.1f)) == 0,
        "a delayed worker takes a bounded step instead of replaying a backlog");
    RibbonMotion fine, coarse;
    RibbonShape a, b;
    for (unsigned i = 0; i < 800; ++i) {
        Features f;
        f.energy = 0.6f;
        f.bass = 0.4f + 0.3f * std::sin(i * 0.006f);
        f.mid = 0.5f - 0.3f * std::sin(i * 0.006f);
        f.treble = 0.2f;
        a = fine.advance(f, 0.01f);
        a = fine.advance(f, 0.01f);
        b = coarse.advance(f, 0.02f);
        check(distance(a, b) < 0.006f, "shape remains stable across normal worker scheduling variation");
    }
    std::cout << "PASS silence, analyzed tones, distinct shapes, sustained stability, continuous band changes, "
        "decay, gate, resume, elapsed-time bounds and scheduling variation; max 10 ms shape step=" << maxStep << '\n';
}
