// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioState.h"
#include <algorithm>
#include <cmath>

AudioState::AudioState(QObject *parent) : QObject(parent), engine(Luma::AudioEngine::acquire()) {
    connect(engine.get(), &Luma::AudioEngine::audioAvailable, this, &AudioState::audioAvailable);
    status.message = QStringLiteral("Connecting to the default audio output.");
    statusTimer.setInterval(1000);
    connect(&statusTimer, &QTimer::timeout, this, [this] {
        const auto current = engine->snapshot().status;
        if (current.message == status.message && current.device == status.device && current.error == status.error
            && current.rate == status.rate && current.channels == status.channels) return;
        status = current;
        Q_EMIT statusChanged();
    });
    statusTimer.start();
}
void AudioState::reportRendering(bool simple) {
    const auto text = simple ? QStringLiteral("Simple rendering is active. Shaders are disabled or unavailable.")
        : QStringLiteral("Shader rendering is active.");
    if (text == rendererStatus) return;
    rendererStatus = text;
    Q_EMIT renderingStatusChanged();
}
QString AudioState::formatDescription() const {
    return status.rate ? QStringLiteral("%1 Hz · %2 channels · float32").arg(status.rate).arg(status.channels) : QString();
}
QVariantMap AudioState::sample(double sensitivity) const {
    const auto snapshot = engine->snapshot();
    sensitivity = std::isfinite(sensitivity) ? std::clamp(sensitivity, 0.5, 2.0) : 1.0;
    const auto scale = [sensitivity](float x) { return std::clamp(double(x) * sensitivity, 0.0, 1.0); };
    const auto &f = snapshot.features;
    return {{"energy", scale(f.energy)}, {"bass", scale(f.bass)}, {"mid", scale(f.mid)},
        {"treble", scale(f.treble)}, {"onset", scale(f.onset)}, {"phase", snapshot.phase},
        {"spectralBalance", f.spectralBalance}, {"trebleShare", f.trebleShare},
        {"arch", snapshot.shape.arch}, {"counterBend", snapshot.shape.counterBend},
        {"bias", snapshot.shape.bias}, {"opening", snapshot.shape.opening},
        {"bassAccent", scale(f.accents[0])}, {"midAccent", scale(f.accents[1])}, {"trebleAccent", scale(f.accents[2])},
        {"time", snapshot.time}, {"rippleAge", f.rippleAge}, {"rippleOrigin", f.rippleOrigin}};
}
