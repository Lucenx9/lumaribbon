// SPDX-License-Identifier: GPL-3.0-or-later
#include "SignalAnalyzer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>

using namespace Luma;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void bounded(Features f) {
    for (float x : {f.spectralBalance, f.trebleShare})
        check(std::isfinite(x) && x >= 0 && x <= 1, "finite bounded timbre descriptors");
}
void feed(SignalAnalyzer &analyzer, unsigned rate, double seconds, float frequency, float level = 0.2f) {
    std::array<StereoFrame, 128> block;
    const unsigned count = unsigned(std::round(rate * seconds));
    auto previous = analyzer.features();
    for (unsigned offset = 0; offset < count; offset += block.size()) {
        const unsigned n = std::min(unsigned(block.size()), count - offset);
        for (unsigned i = 0; i < n; ++i) {
            const float value = level * std::sin(2 * std::numbers::pi * frequency * (offset + i) / rate);
            block[i] = {value, -value}; // Anti-phase stereo must retain its timbre.
        }
        analyzer.feed(std::span(block.data(), n), rate);
        const auto f = analyzer.features();
        bounded(f);
        // The largest supported analysis hop is 64 ms at 8 kHz.
        check(std::abs(f.spectralBalance - previous.spectralBalance) < 0.17f, "no sudden color step");
        previous = f;
    }
}
int main() {
    for (unsigned rate : {8000u, 44100u, 48000u, 96000u, 192000u}) {
        for (unsigned band = 0; band < 3; ++band) {
            SignalAnalyzer analyzer;
            const float hz = std::array{100.f, 1000.f, 3200.f}[band];
            feed(analyzer, rate, 4, hz);
            const auto settled = analyzer.features();
            const float expected = std::array{0.f, 0.65f, 1.f}[band];
            check(std::abs(settled.spectralBalance - expected) < 0.025f, "bass, mids and treble select distinct palette regions");
            check(std::abs(settled.trebleShare - (band == 2 ? 1.f : 0.f)) < 0.025f, "high frequencies control the highlight share");
            feed(analyzer, rate, 3, hz);
            check(std::abs(analyzer.features().spectralBalance - settled.spectralBalance) < 0.001f,
                "stationary sound does not produce a color cycle");
            // Let the window empty, then hold the last hue through the fade.
            feed(analyzer, rate, 0.4, hz, 0);
            const auto fade = analyzer.features();
            feed(analyzer, rate, 4, hz, 0);
            const auto silent = analyzer.features();
            check(silent.spectralBalance == fade.spectralBalance && silent.trebleShare == fade.trebleShare,
                "digital silence holds hue while opacity fades");
            check(silent.energy < 0.001f, "silence still becomes transparent");
            analyzer.decay(5);
            check(analyzer.features().spectralBalance == silent.spectralBalance, "suspended capture holds hue");
            feed(analyzer, rate, 3, 1000, 0.0002f);
            check(analyzer.features().spectralBalance == silent.spectralBalance, "sub-gate noise cannot recolor the ribbon");
            analyzer.discontinuity();
            check(analyzer.features().spectralBalance == silent.spectralBalance, "device discontinuity holds hue");
            analyzer.reset();
            check(analyzer.features().spectralBalance == 0.5f && analyzer.features().trebleShare == 0, "reset restores neutral timbre");
        }
    }
    // Equal timbre at different volumes must settle to the same color.
    SignalAnalyzer quiet, loud;
    for (auto *analyzer : {&quiet, &loud}) {
        std::array<StereoFrame, 480> block;
        const float gain = analyzer == &quiet ? 0.04f : 0.24f;
        for (unsigned step = 0; step < 400; ++step) {
            for (unsigned i = 0; i < block.size(); ++i) {
                const double t = double(step * block.size() + i) / 48000;
                const float value = gain * (std::sin(2 * std::numbers::pi * 100 * t)
                    + 0.4 * std::sin(2 * std::numbers::pi * 1000 * t)
                    + 0.1 * std::sin(2 * std::numbers::pi * 6000 * t));
                block[i] = {value, value};
            }
            analyzer->feed(block, 48000);
        }
    }
    check(std::abs(quiet.features().spectralBalance - loud.features().spectralBalance) < 0.001f,
        "volume normalization does not change timbre");
    check(std::abs(quiet.features().trebleShare - loud.features().trebleShare) < 0.001f,
        "volume normalization does not change highlight share");
    // A phrase changes color within a second, but not in the first analysis hop.
    SignalAnalyzer phrase;
    feed(phrase, 48000, 3, 100);
    const auto bass = phrase.features().spectralBalance;
    feed(phrase, 48000, 0.02, 3200);
    check(phrase.features().spectralBalance < bass + 0.08f, "a new attack does not flash the hue");
    feed(phrase, 48000, 0.8, 3200);
    check(phrase.features().spectralBalance > 0.85f, "a sustained timbre change is visible within a second");
    std::cout << "PASS 15 rate/band cases, smooth response, steady tone, silence, suspension, gate, reset and volume invariance\n";
}
