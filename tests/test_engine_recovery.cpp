// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioEngine.h"
#include "SignalAnalyzer.h"
#include <QCoreApplication>
#include <chrono>
#include <iostream>
#include <thread>
#include <cmath>
#include <stdexcept>

extern "C" void luma_fail_next_fftw_plan();

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    using namespace std::chrono;
    luma_fail_next_fftw_plan();
    auto engine = Luma::AudioEngine::acquire();
    const auto await = [&](auto predicate, seconds timeout) {
        const auto deadline = steady_clock::now() + timeout;
        do {
            if (predicate(engine->snapshot())) return true;
            std::this_thread::sleep_for(milliseconds(10));
        } while (steady_clock::now() < deadline);
        return false;
    };
    const auto failed = [](const auto &snapshot) { return snapshot.status.message.contains("Could not initialize FFTW"); };
    if (!await(failed, seconds(2))) { std::cerr << "Injected initialization failure was not reported\n"; return 1; }
    if (!await([&](const auto &s) { return !s.status.message.isEmpty() && !failed(s); }, seconds(7))) {
        std::cerr << "Analysis did not recover automatically\n"; return 1;
    }
    engine.reset();
    luma_fail_next_fftw_plan();
    engine = Luma::AudioEngine::acquire();
    if (!await(failed, seconds(2))) { std::cerr << "Second injected failure was not reported\n"; return 1; }
    const auto removal = steady_clock::now();
    engine.reset();
    if (steady_clock::now() - removal > milliseconds(500)) {
        std::cerr << "Last-owner removal blocked through the retry backoff\n"; return 1;
    }
    std::cout << "PASS initialization failure, automatic retry and prompt removal during backoff\n";
    Luma::SignalAnalyzer analyzer;
    std::array<Luma::StereoFrame, 512> block;
    unsigned index = 0;
    const auto feedTone = [&](unsigned rate, unsigned count) {
        for (unsigned n = 0; n < count; ++n) {
            for (auto &frame : block) {
                const float value = 0.2f * std::sin(6.283185307179586 * 100 * index++ / rate);
                frame = {value, value};
            }
            analyzer.feed(block, rate);
        }
    };
    feedTone(48000, 100);
    luma_fail_next_fftw_plan();
    bool threw = false;
    try { feedTone(96000, 1); } catch (const std::runtime_error &) { threw = true; }
    if (!threw) { std::cerr << "Injected format-change plan failure was not observed\n"; return 1; }
    feedTone(48000, 20);
    if (analyzer.features().bass < 0.4f) { std::cerr << "The previous plan did not survive a failed replacement\n"; return 1; }
    feedTone(96000, 200);
    if (analyzer.features().bass < 0.4f) { std::cerr << "Plan replacement did not recover\n"; return 1; }
    std::cout << "PASS failed FFT resize retains the previous plan and a later retry succeeds\n";
}
