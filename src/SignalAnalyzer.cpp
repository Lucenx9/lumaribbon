// SPDX-License-Identifier: GPL-3.0-or-later
#include "SignalAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <numbers>
#include <stdexcept>

namespace Luma {
namespace {
// FFTW planning is global and not thread safe, execution of separate plans is.
std::mutex plannerMutex;
constexpr std::array<float, 3> accentPeakDecay{0.065f, 0.045f, 0.035f};
constexpr std::array<float, 3> accentRelease{0.085f, 0.065f, 0.05f};
float follow(float current, float target, float dt, float attack, float release) {
    return target + (current - target) * std::exp(-dt / (target > current ? attack : release));
}
float clean(float value) { return std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f; }
}
SignalAnalyzer::SignalAnalyzer() {
    prepareWindow(Window);
}
void SignalAnalyzer::prepareWindow(unsigned size) {
    std::lock_guard lock(plannerMutex);
    auto nextPlan = fftwf_plan_dft_r2c_1d(size, input.data(), reinterpret_cast<fftwf_complex *>(output.data()), FFTW_ESTIMATE);
    if (!nextPlan) throw std::runtime_error("Could not initialize FFTW");
    if (plan) fftwf_destroy_plan(plan);
    plan = nextPlan;
    windowSize = size;
    hopSize = size / 4;
    for (unsigned i = 0; i < windowSize; ++i)
        window[i] = 0.5f - 0.5f * std::cos(2 * std::numbers::pi_v<float> * i / (windowSize - 1));
}
SignalAnalyzer::~SignalAnalyzer() {
    std::lock_guard lock(plannerMutex);
    fftwf_destroy_plan(plan);
}
void SignalAnalyzer::reset() {
    history.fill({});
    cursor = filled = sinceHop = 0;
    previousMagnitudes.fill(0);
    fluxMean.fill(0);
    bandReference.fill(0);
    accentPeak.fill(0);
    cooldown.fill(0);
    spectrumReady = false;
    onsetCooldown = 0;
    result = {};
    reference = 0.12f;
}
void SignalAnalyzer::discontinuity() {
    filled = sinceHop = 0;
    spectrumReady = false;
    fluxMean.fill(0);
    accentPeak.fill(0);
    cooldown.fill(0.15f);
    result.accents.fill(0);
    result.onset = 0;
    result.rippleAge = 10;
    onsetCooldown = 0.15f;
}
void SignalAnalyzer::feed(std::span<const StereoFrame> frames, uint32_t sampleRate) {
    if (sampleRate < 8000 || sampleRate > 192000) return;
    if (rate != sampleRate) {
        // Preserve roughly the same musical resolution at higher capture rates.
        // Planning happens only on a format change, on the analysis worker.
        unsigned size = Window;
        while (size < MaxWindow && sampleRate > size * 32) size *= 2;
        if (size != windowSize) prepareWindow(size);
        reset();
        rate = sampleRate;
    }
    for (const auto &frame : frames) {
        history[cursor] = {clean(frame.left), clean(frame.right)};
        cursor = (cursor + 1) % windowSize;
        filled = std::min(filled + 1, windowSize);
        if (++sinceHop >= hopSize) {
            sinceHop = 0;
            if (filled == windowSize) analyze();
        }
    }
}
void SignalAnalyzer::analyze() {
    const float dt = float(hopSize) / rate;
    double sum = 0;
    float peak = 0;
    powers.fill(0);
    // Add spectral POWER, not waveforms: anti-phase stereo remains visible.
    for (unsigned channel = 0; channel < 2; ++channel) {
        float mean = 0;
        for (unsigned i = 0; i < windowSize; ++i) mean += channel ? history[i].right : history[i].left;
        mean /= windowSize;
        for (unsigned i = 0; i < windowSize; ++i) {
            const auto &frame = history[(cursor + i) % windowSize];
            const float value = (channel ? frame.right : frame.left) - mean;
            sum += double(value) * value;
            peak = std::max(peak, std::abs(value));
            input[i] = value * window[i];
        }
        fftwf_execute(plan);
        for (unsigned i = 1; i <= windowSize / 2; ++i)
            powers[i] += output[i][0] * output[i][0] + output[i][1] * output[i][1];
    }
    const float rms = std::sqrt(sum / (2 * windowSize));
    // Absolute gate precedes normalization. It never learns the noise floor upward.
    constexpr float gateLow = 0.00032f; // -70 dBFS
    constexpr float gateHigh = 0.001f;  // -60 dBFS
    float gate = std::clamp((rms - gateLow) / (gateHigh - gateLow), 0.0f, 1.0f);
    gate = gate * gate * (3 - 2 * gate);
    if (gate > 0.99f)
        reference = follow(reference, std::max(0.06f, rms), dt, 0.35f, 12.0f);
    const float gain = 1 / std::max(reference, 0.06f);
    const auto normalize = [gain, gate](float amplitude) {
        return gate * std::clamp(1 - std::exp(-1.4f * amplitude * gain), 0.0f, 1.0f);
    };
    float bands[3]{}, flux[3]{};
    // Positive spectral changes reveal new notes even when total RMS is steady.
    // Weight the neighboring-bin maximum by pitch, so a whole FFT bin does
    // not erase a bass semitone. This is a soft filter, not pitch tracking.
    const float spectralScale = 1.0f / (windowSize * windowSize * 0.375f);
    for (unsigned i = 1; i <= windowSize / 2; ++i) {
        const float hz = float(i) * rate / windowSize;
        if (hz < 35 || hz > 16000) continue;
        const unsigned band = hz < 250 ? 0 : (hz < 2500 ? 1 : 2);
        bands[band] += powers[i];
        const float magnitude = std::sqrt(powers[i] * spectralScale);
        const float localMaximum = std::max({previousMagnitudes[i - 1], previousMagnitudes[i],
            previousMagnitudes[std::min(i + 1, windowSize / 2)]});
        const float filterAmount = std::min(1.0f, hz * 0.012f * windowSize / rate);
        const float previous = previousMagnitudes[i] + filterAmount * (localMaximum - previousMagnitudes[i]);
        const float rise = std::max(0.0f, magnitude - previous);
        flux[band] += rise * rise;
    }
    for (unsigned i = 0; i <= windowSize / 2; ++i)
        previousMagnitudes[i] = std::sqrt(powers[i] * spectralScale);
    // Hann window mean-square ~3/8; one-sided and two channels cancel factor 2.
    result.bass = follow(result.bass, normalize(std::sqrt(bands[0] * spectralScale)), dt, 0.055f, 0.38f);
    result.mid = follow(result.mid, normalize(std::sqrt(bands[1] * spectralScale)), dt, 0.085f, 0.32f);
    result.treble = follow(result.treble, normalize(std::sqrt(bands[2] * spectralScale)), dt, 0.04f, 0.24f);
    result.energy = follow(result.energy, normalize(rms), dt, 0.07f, 0.48f);
    if (gate > 0.99f) {
        // Use relative spectral amplitudes before display normalization. Weight
        // quieter mids/highs so a bass-heavy mix can still change its color.
        const float low = std::sqrt(bands[0]);
        const float middle = 1.8f * std::sqrt(bands[1]);
        const float high = 3.0f * std::sqrt(bands[2]);
        const float total = low + middle + high;
        if (total > 0) {
            const float balance = (0.65f * middle + high) / total;
            result.spectralBalance = follow(result.spectralBalance, balance, dt, 0.35f, 0.35f);
            result.trebleShare = follow(result.trebleShare, high / total, dt, 0.55f, 0.55f);
        }
    }
    // Hold these ratios below the gate and during missing-input decay. Unequal
    // band release times must not recolor a silent ribbon as it fades away.
    onsetCooldown = std::max(0.0f, onsetCooldown - dt);
    result.onset *= std::exp(-dt / 0.19f);
    result.rippleAge = std::min(10.0f, result.rippleAge + dt);
    float strongest = 0;
    float originSum = 0, originWeight = 0;
    for (unsigned band = 0; band < 3; ++band) {
        const float level = std::sqrt(bands[band] * spectralScale);
        if (gate > 0.99f)
            bandReference[band] = follow(bandReference[band], level, dt, 0.15f, 4.0f);
        const float novelty = spectrumReady ? std::sqrt(flux[band]) : 0;
        const float threshold = 0.0012f + 1.8f * fluxMean[band] + 0.025f * bandReference[band];
        const float denominator = std::max({0.012f, bandReference[band] * 0.18f, reference * 0.025f});
        const float strength = gate * (1 - std::exp(-2.5f * std::max(0.0f, novelty - threshold) / denominator));
        // A symmetric mean must fall between dense notes, unlike a peak hold.
        fluxMean[band] = follow(fluxMean[band], novelty, dt, 0.25f, 0.25f);
        cooldown[band] = std::max(0.0f, cooldown[band] - dt);
        accentPeak[band] *= std::exp(-dt / accentPeakDecay[band]);
        if (strength > 0.12f) {
            accentPeak[band] = std::max(accentPeak[band], strength);
            if (cooldown[band] == 0) {
                cooldown[band] = 0.09f;
                strongest = std::max(strongest, strength);
                const float weight = strength * strength;
                originSum += weight * (0.32f + 0.18f * band);
                originWeight += weight;
            }
        }
        // A short rise and a longer release retain accents without hard flashes.
        result.accents[band] = follow(result.accents[band], accentPeak[band], dt, 0.012f, accentRelease[band]);
    }
    if (strongest > 0.12f && onsetCooldown == 0) {
        result.onset = strongest;
        result.rippleAge = 0;
        // Freeze the spectral origin for this entire ripple. Mixed attacks sit
        // between bands; ongoing changes must not drag an existing wave around.
        result.rippleOrigin = originSum / originWeight;
        onsetCooldown = 0.16f;
    }
    spectrumReady = true;
    result.rms = rms;
    result.peak = std::min(peak, 1.0f);
}
void SignalAnalyzer::decay(float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0) return;
    result.energy *= std::exp(-seconds / 0.48f);
    result.bass *= std::exp(-seconds / 0.38f);
    result.mid *= std::exp(-seconds / 0.32f);
    result.treble *= std::exp(-seconds / 0.24f);
    result.onset *= std::exp(-seconds / 0.19f);
    for (unsigned band = 0; band < 3; ++band) {
        accentPeak[band] *= std::exp(-seconds / accentPeakDecay[band]);
        result.accents[band] *= std::exp(-seconds / accentRelease[band]);
        cooldown[band] = std::max(0.0f, cooldown[band] - seconds);
    }
    result.rippleAge = std::min(10.0f, result.rippleAge + seconds);
    result.rms = result.peak = 0;
    onsetCooldown = std::max(0.0f, onsetCooldown - seconds);
    // No old FFT history after a suspended stream resumes.
    filled = sinceHop = 0;
    spectrumReady = false;
}
}
