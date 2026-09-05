// SPDX-License-Identifier: GPL-3.0-or-later
#include "SignalAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <functional>
#include <string_view>
#include <vector>

using namespace Luma;
namespace {
void check(bool condition, const char *message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
float sine(double hz, double time) { return std::sin(2 * std::numbers::pi * hz * time); }
float burst(double age, double decay = 16) {
    return age >= 0 && age < 0.2 ? std::min(1.0, age / 0.004) * std::exp(-age * decay) : 0;
}
struct Point { double time; Features f; };
using Signal = std::function<float(double)>;
std::vector<Point> analyze(const Signal &signal, unsigned rate = 48000, double seconds = 4) {
    SignalAnalyzer analyzer;
    std::array<StereoFrame, 128> block;
    std::vector<Point> points;
    for (unsigned offset = 0; offset < unsigned(rate * seconds); offset += block.size()) {
        for (unsigned i = 0; i < block.size(); ++i) {
            const float sample = signal(double(offset + i) / rate);
            block[i] = {sample, -sample}; // Include anti-phase stereo in every case.
        }
        analyzer.feed(block, rate);
        const auto f = analyzer.features();
        for (float value : {f.energy, f.bass, f.mid, f.treble, f.onset, f.accents[0], f.accents[1], f.accents[2]})
            check(std::isfinite(value) && value >= 0 && value <= 1, "finite normalized musical features");
        check(std::isfinite(f.rippleAge) && f.rippleAge >= 0 && f.rippleAge <= 10, "bounded ripple age");
        check(std::isfinite(f.rippleOrigin) && f.rippleOrigin >= 0.319f && f.rippleOrigin <= 0.681f, "bounded ripple origin");
        if (!points.empty() && f.rippleAge >= points.back().f.rippleAge)
            check(f.rippleOrigin == points.back().f.rippleOrigin, "an existing ripple keeps its spectral origin");
        points.push_back({double(offset + block.size()) / rate, f});
    }
    return points;
}

// A quiet high note enters over a sustained, much louder low note.
// Its identity is audible in the spectrum even when total RMS barely changes.
void maskedAccent() {
    SignalAnalyzer analyzer;
    std::array<StereoFrame, 128> block;
    float peak = 0;
    for (unsigned offset = 0; offset < 48000 * 3; offset += block.size()) {
        for (unsigned i = 0; i < block.size(); ++i) {
            const double t = double(offset + i) / 48000;
            const double age = t - 2;
            const double envelope = burst(age);
            const float sample = 0.32f * sine(94, t) + 0.045f * envelope * sine(4800, t);
            block[i] = {sample, sample};
        }
        analyzer.feed(block, 48000);
        if (offset >= 96000 && offset < 105600) peak = std::max(peak, analyzer.features().onset);
    }
    std::cout << "Masked high accent: peak=" << peak << '\n';
    check(peak > 0.12f, "a high accent over a sustained bass must trigger a visible response");
}

void bandAccents() {
    for (unsigned rate : {8000u, 44100u, 48000u, 96000u, 192000u}) {
        for (unsigned band = 0; band < 3; ++band) {
            const double frequency = band == 0 ? 94 : band == 1 ? 740 : 3200;
            const double bed = band == 0 ? 740 : 94;
            auto points = analyze([&](double t) {
                return 0.32f * sine(bed, t) + 0.065f * burst(t - 2, 10) * sine(frequency, t);
            }, rate);
            std::array<float, 3> peaks{};
            double latency = 10;
            bool located = false;
            for (const auto &[t, f] : points) if (t >= 2 && t < 2.45) {
                for (unsigned b = 0; b < 3; ++b) peaks[b] = std::max(peaks[b], f.accents[b]);
                if (f.accents[band] > 0.12f) latency = std::min(latency, t - 2);
                if (f.rippleAge == 0 && f.onset > 0.12f) {
                    check(std::abs(f.rippleOrigin - (0.32f + 0.18f * band)) < 0.04f, "a band's attack starts in its spectral region");
                    located = true;
                }
            }
            check(located, "a quieter attack produces a located ripple over a sustained other band");
            std::cout << "rate=" << rate << " band=" << band << " peaks=" << peaks[0] << ',' << peaks[1] << ',' << peaks[2]
                << " latency-ms=" << latency * 1000 << '\n';
            check(peaks[band] > 0.12f, "each band hears a quieter accent over a sustained other band");
            check(peaks[band] > 3 * peaks[(band + 1) % 3] && peaks[band] > 3 * peaks[(band + 2) % 3], "the accent stays in its musical band");
            check(latency < (rate < 44100 ? 0.3 : 0.1), "accents respond within the window and display budget");
        }
    }
}
void timbreAndRestraint() {
    const auto change = analyze([](double t) {
        return 0.3f * sine(94, t) + 0.12f * sine(t < 2 ? 740 : 1100, t);
    });
    float accent = 0, before = 0, after = 0;
    for (const auto &[t, f] : change) {
        if (t > 1.8 && t < 1.9) before = f.rms;
        if (t > 2.2 && t < 2.3) after = f.rms;
        if (t >= 2 && t < 2.2) accent = std::max(accent, f.accents[1]);
    }
    check(std::abs(after - before) < 0.01f && accent > 0.12f, "a new mid note is visible at almost constant RMS");
    std::cout << "Equal-level note change: mid accent=" << accent << " RMS difference=" << after - before << '\n';

    for (unsigned scenario = 0; scenario < 5; ++scenario) {
        uint32_t noiseState = 1234567;
        const auto points = analyze([&](double t) {
            noiseState = noiseState * 1664525u + 1013904223u;
            const float noise = float(noiseState >> 8) / 8388608.0f - 1;
            if (scenario == 0) return 0.25f * sine(94, t) + 0.12f * sine(740, t) + 0.04f * sine(4800, t);
            if (scenario == 1) return float(0.2 * std::sin(2 * std::numbers::pi * 740 * t + 1.2 * std::sin(2 * std::numbers::pi * 6 * t)));
            if (scenario == 2) return 0.04f * noise;
            if (scenario == 4) return float(0.1 + 0.1 * std::clamp((t - 2) / 2, 0.0, 1.0)) * sine(740, t);
            return 0.0002f * noise;
        }, 48000, 6);
        float peak = 0;
        for (const auto &[t, f] : points) if (t > 2) {
            for (float value : f.accents) peak = std::max(peak, value);
            if (scenario == 3) check(f.energy == 0 && f.onset == 0, "sub-gate noise remains invisible");
        }
        std::cout << "Steady scenario=" << scenario << " accent maximum=" << peak << '\n';
        check(peak < 0.08f, "sustained tones, small vibrato, stationary noise and a slow swell do not invent repeated accents");
    }
}
void repetitionAndGaps() {
    const auto points = analyze([](double t) {
        const double age = t >= 2 && t < 4 ? std::fmod(t - 2, 0.25) : -1;
        return 0.26f * sine(94, t) + 0.06f * burst(age, 28) * sine(4800, t);
    }, 48000, 5);
    unsigned detected = 0;
    for (unsigned beat = 0; beat < 8; ++beat) {
        const double start = 2 + beat * 0.25;
        float peak = 0;
        for (const auto &[t, f] : points) if (t >= start && t < start + 0.12) peak = std::max(peak, f.accents[2]);
        detected += peak > 0.12f;
    }
    std::cout << "Repeated treble accents=" << detected << "/8\n";
    check(detected == 8, "repeated accents remain visible after the adaptive threshold settles");
    const auto closeAccents = analyze([](double t) {
        return 0.24f * sine(740, t) + 0.1f * burst(t - 2) * sine(94, t)
            + 0.04f * burst(t - 2.06) * sine(4800, t);
    });
    float highAfterLow = 0;
    for (const auto &[t, f] : closeAccents) if (t >= 2.06 && t < 2.2) highAfterLow = std::max(highAfterLow, f.accents[2]);
    check(highAfterLow > 0.12f, "a bass attack must not lock out the following treble accent");
    SignalAnalyzer analyzer;
    std::array<StereoFrame, 512> block{};
    unsigned sample = 0;
    const auto feed = [&](unsigned blocks) {
        for (unsigned n = 0; n < blocks; ++n) {
            for (auto &frame : block) { const float value = 0.2f * sine(740, double(sample++) / 48000); frame = {value, value}; }
            analyzer.feed(block, 48000);
        }
    };
    feed(100);
    analyzer.discontinuity();
    feed(8);
    check(analyzer.features().onset == 0, "a new capture generation does not create an attack");
    analyzer.decay(5);
    for (float value : analyzer.features().accents) check(value < 0.001f, "accents decay on missing input");
    block.fill({});
    for (unsigned n = 0; n < 50; ++n) analyzer.feed(block, 48000);
    check(analyzer.features().onset < 0.001f && analyzer.features().energy < 0.001f, "resuming in silence has no stale accents");
}
// A capture generation break must not carry the previous device's loudness
// into the new one: quiet accents stay visible right after a device switch.
void accentAfterDeviceSwitch() {
    SignalAnalyzer analyzer;
    std::array<StereoFrame, 512> block;
    unsigned sample = 0;
    float peak = 0;
    const auto feed = [&](unsigned blocks, float amplitude, double hz) {
        for (unsigned n = 0; n < blocks; ++n) {
            for (auto &frame : block) {
                const float value = amplitude * sine(hz, double(sample++) / 48000);
                frame = {value, value};
            }
            analyzer.feed(block, 48000);
            peak = std::max(peak, analyzer.features().accents[0]);
        }
    };
    feed(180, 0.34f, 94);
    analyzer.discontinuity();
    feed(4, 0.0f, 94);
    peak = 0;
    feed(40, 0.02f, 94);
    std::cout << "Quiet bass accent after a device switch: peak=" << peak << '\n';
    check(peak > 0.12f, "a quiet bass attack is heard after switching to a quieter device");
}
void closeNotes() {
    bool passed = true;
    for (unsigned rate : {44100u, 48000u, 96000u, 192000u}) {
        for (double hz : {82.4069, 110.0, 220.0, 440.0, 740.0}) {
            const auto points = analyze([=](double t) {
                // Continuous phase at the note change: no artificial click.
                const double cycles = hz * std::min(t, 2.0) + hz * std::exp2(1.0 / 12) * std::max(0.0, t - 2);
                return float(0.18 * std::sin(2 * std::numbers::pi * cycles)) + 0.24f * sine(hz < 250 ? 740 : 94, t);
            }, rate);
            const unsigned band = hz < 250 ? 0 : 1;
            float accent = 0, resting = 0;
            for (const auto &[t, f] : points) {
                if (t > 1.5 && t < 2) resting = std::max(resting, f.accents[band]);
                if (t >= 2 && t < 2.15) accent = std::max(accent, f.accents[band]);
            }
            std::cout << "Semitone rate=" << rate << " hz=" << hz << " resting=" << resting << " accent=" << accent << '\n';
            passed &= accent > 0.12f && resting < 0.08f;
        }
    }
    check(passed, "nearby notes at constant amplitude must remain distinct from a sustained tone");
}
void fastAccents() {
    bool passed = true;
    for (unsigned rate : {44100u, 48000u, 96000u, 192000u}) {
        for (double hz : {740.0, 4800.0}) {
            const unsigned band = hz < 2500 ? 1 : 2;
            for (unsigned perSecond : {8u, 12u}) {
                const double interval = 1.0 / perSecond;
                const auto points = analyze([=](double t) {
                    const double age = t >= 2 && t < 4 ? std::fmod(t - 2, interval) : -1;
                    return 0.26f * sine(94, t) + 0.065f * burst(age, 60) * sine(hz, t);
                }, rate, 4.5);
                unsigned distinct = 0;
                float leastContrast = 1;
                for (unsigned note = 1; note < perSecond * 2; ++note) {
                    const double start = 2 + note * interval;
                    float peak = 0, trough = 1;
                    for (const auto &[t, f] : points) {
                        if (t >= start && t < start + interval * 0.7) peak = std::max(peak, f.accents[band]);
                        if (t >= start - interval * 0.3 && t < start + 0.005) trough = std::min(trough, f.accents[band]);
                    }
                    const float contrast = peak - trough;
                    distinct += peak > 0.12f && contrast > 0.08f;
                    leastContrast = std::min(leastContrast, contrast);
                }
                std::cout << "Fast accents rate=" << rate << " hz=" << hz << " notes/s=" << perSecond << " distinct=" << distinct
                    << '/' << perSecond * 2 - 1 << " least contrast=" << leastContrast << '\n';
                passed &= distinct == perSecond * 2 - 1;
            }
        }
    }
    check(passed, "fast repeated accents need a visible rise between successive notes");
}
void phraseAndVibrato() {
    for (unsigned rate : {44100u, 48000u, 96000u, 192000u}) {
        double phase = 0;
        constexpr std::array<int, 8> notes{41, 43, 42, 40, 43, 45, 44, 43};
        const auto phrase = analyze([&](double t) {
            const int index = int(std::floor((t - 2) / 0.3));
            const int note = index < 0 ? 40 : notes[std::min(index, 7)];
            phase += 440 * std::exp2((note - 69) / 12.0) / rate;
            return float(0.18 * std::sin(2 * std::numbers::pi * phase)) + 0.24f * sine(740, t);
        }, rate, 5);
        unsigned detected = 0;
        float least = 1;
        for (unsigned n = 0; n < notes.size(); ++n) {
            float peak = 0;
            for (const auto &[t, f] : phrase)
                if (t >= 2 + n * 0.3 && t < 2.15 + n * 0.3) peak = std::max(peak, f.accents[0]);
            detected += peak > 0.12f;
            least = std::min(least, peak);
        }
        std::cout << "Bass phrase rate=" << rate << " notes=" << detected << "/8 minimum=" << least << '\n';
        check(detected == 8, "ascending and descending bass notes remain visible throughout a phrase");
        for (double hz : {82.4069, 110.0, 220.0, 740.0}) {
            const auto points = analyze([=](double t) {
                return float(0.18 * std::sin(2 * std::numbers::pi * hz * t
                    + hz * 0.007 / 6 * std::sin(2 * std::numbers::pi * 6 * t)));
            }, rate, 5);
            float peak = 0;
            for (const auto &[t, f] : points) if (t > 2)
                for (float a : f.accents) peak = std::max(peak, a);
            std::cout << "Small vibrato rate=" << rate << " hz=" << hz << " maximum=" << peak << '\n';
            check(peak < 0.08f, "small vibrato must not become a sequence of accents");
        }
    }
}
}
int main(int argc, char **argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--close-notes") { closeNotes(); return 0; }
    if (argc == 2 && std::string_view(argv[1]) == "--fast-accents") { fastAccents(); return 0; }
    maskedAccent(); bandAccents(); timbreAndRestraint(); repetitionAndGaps(); accentAfterDeviceSwitch();
    closeNotes(); fastAccents(); phraseAndVibrato();
}
