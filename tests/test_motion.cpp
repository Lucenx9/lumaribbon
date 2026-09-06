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
std::array<float, 6> values(RibbonShape s) { return {s.arch, s.counterBend, s.bias, s.opening, s.lift, s.lean}; }
float distance(RibbonShape a, RibbonShape b) {
    float result = 0;
    for (unsigned i = 0; i < values(a).size(); ++i) result = std::max(result, std::abs(values(a)[i] - values(b)[i]));
    return result;
}
void finite(RibbonShape s) {
    for (float v : values(s)) check(std::isfinite(v) && std::abs(v) < 1.5f, "finite bounded shape");
    check(s.opening >= 0.14f && s.opening <= 1.01f, "bounded filament opening");
    check(std::abs(s.lift) <= 1.001f && std::abs(s.lean) <= 1.001f, "bounded slow base movement");
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
        for (unsigned tick = 0; tick < 2400; ++tick) {
            for (auto &frame : block) {
                const float sample = 0.2f * std::sin(2 * std::numbers::pi * frequencies[band] * index++ / 48000);
                frame = {sample, -sample}; // Stereo power must survive phase cancellation.
            }
            analyzer.feed(block, 48000);
            const auto current = motion.advance(analyzer.features(), 0.01f);
            finite(current);
            maxStep = std::max(maxStep, distance(current, previous));
            previous = current;
            if (tick == 2299) nearEnd = current;
        }
        settled[band] = previous;
        std::cout << "tone=" << frequencies[band] << " settled change=" << distance(previous, nearEnd)
                  << " lift=" << previous.lift << " lean=" << previous.lean << '\n';
        check(distance(previous, nearEnd) < 0.003f, "a sustained tone settles instead of looping shapes");
    }
    check(settled[0].arch > 0.65f && settled[0].bias < -0.4f, "bass produces a broad arch");
    check(settled[1].counterBend > 0.8f, "mids produce a distinct counter-bend");
    check(settled[2].arch < -0.55f && settled[2].bias > 0.4f, "highs produce an opposite shallow arch");
    check(maxStep < 0.055f, "abrupt band switches stay continuous at worker cadence");

    // A crescendo must move the base even if the band proportions do not change.
    RibbonMotion phrases;
    Features level;
    level.energy = 0.25f;
    level.bass = level.mid = 0.2f;
    level.treble = 0.1f;
    level.rms = 0.03f;
    for (unsigned i = 0; i < 1600; ++i) phrases.advance(level, 0.01f);
    const auto rest = phrases.advance(level, 0.01f);
    check(std::abs(rest.lift) < 0.001f && std::abs(rest.lean) < 0.001f, "uniform input has no slow drift");
    level.energy = 0.75f;
    level.bass = level.mid = 0.6f;
    level.treble = 0.3f;
    level.rms = 0.2f;
    const auto start = phrases.advance(level, 0.01f);
    check(std::abs(start.lift - rest.lift) < 0.001f, "the base cannot jump on an attack");
    RibbonShape rise;
    for (unsigned i = 0; i < 150; ++i) rise = phrases.advance(level, 0.01f);
    check(rise.lift > 0.6f && std::abs(rise.lean) < 0.001f, "a balanced crescendo lifts without tilting");
    for (unsigned i = 0; i < 2000; ++i) phrases.advance(level, 0.01f);
    level.energy = 0.25f;
    level.bass = level.mid = 0.2f;
    level.treble = 0.1f;
    level.rms = 0.03f;
    RibbonShape fall;
    for (unsigned i = 0; i < 150; ++i) fall = phrases.advance(level, 0.01f);
    check(fall.lift < -0.6f, "a falling phrase lowers the base gradually");
    // At fixed loudness, a timbre change only drives the lean.
    RibbonMotion midPhrase, bassPhrase;
    Features neutral;
    neutral.energy = 0.6f;
    neutral.bass = neutral.mid = 0.35f;
    neutral.treble = 0.2f;
    neutral.rms = 0.12f;
    for (unsigned i = 0; i < 1600; ++i) {
        midPhrase.advance(neutral, 0.01f);
        bassPhrase.advance(neutral, 0.01f);
    }
    Features lead = neutral, lowLead = neutral;
    lead.mid = lowLead.bass = 0.6f;
    lead.bass = lowLead.mid = 0.1f;
    lead.spectralBalance = 0.8f;
    lowLead.spectralBalance = 0.2f;
    RibbonShape midRise, bassRise;
    for (unsigned i = 0; i < 150; ++i) {
        midRise = midPhrase.advance(lead, 0.01f);
        bassRise = bassPhrase.advance(lowLead, 0.01f);
    }
    check(midRise.lean > 0.5f && bassRise.lean < -0.5f, "bass and mid phrases lean in opposite directions");
    check(std::abs(midRise.lift) < 0.001f && std::abs(bassRise.lift) < 0.001f, "steady energy does not invent a lift");
    RibbonMotion gainChanges;
    for (unsigned i = 0; i < 1000; ++i) {
        Features f = neutral;
        f.energy = f.bass = f.mid = f.treble = 0.25f + 0.65f * i / 1000;
        const auto shape = gainChanges.advance(f, 0.01f);
        check(shape.lift == 0 && shape.lean == 0, "display normalization cannot move the base at fixed RMS and timbre");
    }
    neutral.accents = {1, 1, 1};
    RibbonMotion accented, unaccented;
    for (unsigned i = 0; i < 600; ++i) {
        const auto withAccents = accented.advance(neutral, 0.01f);
        neutral.accents = {};
        check(distance(withAccents, unaccented.advance(neutral, 0.01f)) == 0,
            "short accents do not drive broad base motion");
        neutral.accents = {1, 1, 1};
    }

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

    for (auto field : {&Features::energy, &Features::bass, &Features::mid, &Features::treble,
                      &Features::rms, &Features::spectralBalance}) {
        for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}) {
            Features poisoned = loud;
            poisoned.*field = invalid;
            const auto shape = motion.advance(poisoned, 0.01f);
            finite(shape);
            check(distance(previous, shape) == 0, "each non-finite feature holds the current shape");
        }
    }
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
        f.rms = 0.08f + 0.04f * std::sin(i * 0.006f);
        f.spectralBalance = 0.5f + 0.3f * std::sin(i * 0.006f);
        a = fine.advance(f, 0.01f);
        a = fine.advance(f, 0.01f);
        b = coarse.advance(f, 0.02f);
        check(distance(a, b) < 0.006f, "shape remains stable across normal worker scheduling variation");
    }
    std::cout << "PASS silence, analyzed tones, distinct shapes, sustained stability, continuous band changes, "
        "slow crescendo lift, phrase lean, decay, gate, resume, elapsed-time bounds and scheduling variation; "
        "max 10 ms shape step=" << maxStep << '\n';
}
