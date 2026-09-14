// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioEngine.h"
#include <QCoreApplication>
#include <QTimer>
#include <iostream>
#include <cmath>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    auto engine = Luma::AudioEngine::acquire();
    auto second = Luma::AudioEngine::acquire();
    if (engine != second) return 3;
    second.reset();
    float maximum = 0;
    int samples = 0;
    const int durationIndex = args.indexOf("--seconds");
    const int seconds = durationIndex >= 0 && durationIndex + 1 < args.size() ? std::clamp(args[durationIndex + 1].toInt(), 1, 120) : 6;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        const auto s = engine->snapshot();
        for (float f : {s.features.energy, s.features.bass, s.features.mid, s.features.treble, s.features.onset,
                 s.features.accents[0], s.features.accents[1], s.features.accents[2],
                 s.features.spectralBalance, s.features.trebleShare})
            if (!std::isfinite(f) || f < 0 || f > 1) app.exit(2);
        maximum = std::max(maximum, s.features.energy);
        std::cout << "energy=" << s.features.energy << " bass=" << s.features.bass << " mid=" << s.features.mid
            << " treble=" << s.features.treble << " accents=" << s.features.accents[0] << ',' << s.features.accents[1] << ',' << s.features.accents[2]
            << " spectralBalance=" << s.features.spectralBalance << " trebleShare=" << s.features.trebleShare
            << " rate=" << s.status.rate << " channels=" << s.status.channels
            << " dropped=" << s.dropped << " expired=" << s.expired << " error=" << s.status.error << " device=" << s.status.device.toStdString()
            << " status=" << s.status.message.toStdString()
            << " detail=" << s.status.detail.simplified().toStdString() << std::endl;
        if (++samples >= seconds * 2) app.exit(args.contains("--expect-audio") && maximum < 0.03f ? 1 : 0);
    });
    timer.start(500);
    const int result = app.exec();
    std::weak_ptr<Luma::AudioEngine> weak = engine;
    engine.reset();
    if (!weak.expired()) return 4;
    std::cout << "maximum=" << maximum << " shared-engine=yes released=yes\n";
    return result;
}
