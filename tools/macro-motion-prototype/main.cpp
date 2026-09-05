// SPDX-License-Identifier: GPL-3.0-or-later
// Historical study: compare the candidate with the earlier shader on identical input.
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QTimer>
#include <QElapsedTimer>
#include <QDir>
#include <QImage>
#include "AudioEngine.h"
#include "SignalAnalyzer.h"
#include "MacroMotion.h"
#include <fstream>
#include <functional>
#include <iostream>
#include <numbers>

namespace {
constexpr double Duration = 26;

QString section(double time) {
    if (time < 4) return QStringLiteral("Low notes and bass pulses");
    if (time < 8) return QStringLiteral("Mid-register phrase");
    if (time < 12) return QStringLiteral("High-register phrase");
    if (time < 17) return QStringLiteral("Changing blend");
    if (time < 20) return QStringLiteral("Quiet passage");
    if (time < 22) return QStringLiteral("Full return");
    return QStringLiteral("Silence and fade");
}

float syntheticSample(double time) {
    const auto sine = [time](double hz) { return std::sin(2 * std::numbers::pi * hz * time); };
    const double age = std::fmod(time, 0.72);
    const double pulse = std::min(1.0, age / 0.012) * std::exp(-age * 10);
    if (time < 4) return float((0.11 + 0.1 * pulse) * sine(94) + 0.015 * sine(740));
    if (time < 8) return float(0.02 * sine(94) + (0.11 + 0.065 * std::sin(time * 2)) * sine(740));
    if (time < 12) return float(0.015 * sine(740) + (0.10 + 0.025 * std::sin(time * 3)) * sine(4800));
    if (time < 17) {
        const double mix = 0.5 + 0.5 * std::sin((time - 12) * 1.4);
        return float((0.02 + 0.16 * mix) * sine(94) + (0.02 + 0.12 * (1 - mix)) * sine(740)
            + 0.065 * pulse * sine(4800));
    }
    if (time < 20) return float(0.006 * (0.8 + 0.2 * std::sin(time)) * sine(740));
    if (time < 22) return float((0.10 + 0.12 * pulse) * sine(94) + 0.065 * sine(740) + 0.03 * sine(4800));
    return 0;
}

struct Study {
    Luma::SignalAnalyzer analyzer;
    std::shared_ptr<Luma::AudioEngine> engine;
    Luma::AudioSnapshot snapshot;
    MacroMotion motion;
    uint64_t sampleIndex = 0;

    void advanceSynthetic(unsigned count, std::ofstream *pcm = nullptr) {
        std::array<Luma::StereoFrame, 480> block;
        for (unsigned i = 0; i < count; ++i) {
            const float sample = syntheticSample(double(sampleIndex++) / 48000);
            block[i] = {sample, sample};
        }
        if (pcm) pcm->write(reinterpret_cast<const char *>(block.data()), count * sizeof(Luma::StereoFrame));
        analyzer.feed(std::span(block.data(), count), 48000);
        snapshot.features = analyzer.features();
        const float dt = float(count) / 48000;
        snapshot.time = double(sampleIndex) / 48000;
        if (snapshot.features.energy > 0.001f)
            snapshot.phase = std::fmod(snapshot.phase + dt * (0.08 + 0.55 * snapshot.features.energy
                + 0.65 * snapshot.features.mid), 200 * std::numbers::pi);
        motion.advance(snapshot.features, dt);
    }

    void advanceLive(float dt) {
        snapshot = engine->snapshot();
        motion.advance(snapshot.features, dt);
    }

    QVariantMap frame() const {
        const auto &f = snapshot.features;
        return {{"energy", f.energy < 0.001f ? 0.0f : f.energy}, {"bass", f.bass}, {"mid", f.mid}, {"treble", f.treble},
            {"onset", f.onset}, {"phase", snapshot.phase}, {"rippleAge", f.rippleAge}, {"rippleOrigin", f.rippleOrigin},
            {"spectralBalance", f.spectralBalance}, {"trebleShare", f.trebleShare},
            {"bassAccent", f.accents[0]}, {"midAccent", f.accents[1]}, {"trebleAccent", f.accents[2]},
            {"arch", motion.value[0]}, {"counterBend", motion.value[1]}, {"bias", motion.value[2]}, {"opening", motion.value[3]}};
    }
};
}

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.contains("--help")) {
        std::cout << "Luma Ribbon main-shape prototype. No installation or audio playback.\n"
                     "Default: live default-output monitor. --synthetic: repeat a generated signal study.\n"
                     "--capture FILE: save one frame after 5 seconds and exit.\n"
                     "--synthetic --at SECONDS --capture FILE: deterministic still, 0..26 s.\n"
                     "--synthetic --record DIRECTORY: 26-second PNG sequence, synthetic PCM and trace.\n"
                     "--trace FILE: write the displayed features and shape state, including live input.\n"
                     "--full: include attacks and dynamic color. --reduced: reduced motion.\n"
                     "--fallback: simple renderer. --palette 0|1|2|3|4|5: Aurora, Ember, Ice, Grove, Iris, Coral.\n";
        return 0;
    }
    const auto option = [&args](const QString &name) {
        const int index = args.indexOf(name);
        return index < 0 ? QString() : args.value(index + 1);
    };
    const bool synthetic = args.contains("--synthetic");
    const bool frozen = args.contains("--at");
    const QString capture = option("--capture");
    const QString recording = option("--record");
    bool valid = false;
    const double at = option("--at").toDouble(&valid);
    if ((frozen && (!synthetic || !valid || at < 0 || at > Duration || !std::isfinite(at)))
        || (!recording.isEmpty() && (!synthetic || frozen))
        || (args.contains("--capture") && capture.isEmpty())
        || (args.contains("--record") && recording.isEmpty())) {
        std::cerr << "Invalid arguments. See --help.\n";
        return 1;
    }
    Study study;
    if (synthetic) {
        if (frozen) for (int i = 0; i < int(std::round(at * 100)); ++i) study.advanceSynthetic(480);
    } else study.engine = Luma::AudioEngine::acquire();
    QQuickView view;
    view.setTitle(QStringLiteral("Luma Ribbon · main-shape prototype"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setInitialProperties({{"frame", study.frame()}, {"syntheticInput", synthetic},
        {"isolateShape", !args.contains("--full")}, {"reduceMotion", args.contains("--reduced")},
        {"simpleRendering", args.contains("--fallback")}, {"paletteChoice", std::clamp(option("--palette").toInt(), 0, 5)}});
    view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_PROTOTYPE_DIR "/Study.qml")));
    if (view.status() != QQuickView::Ready) return 2;
    view.show();

    std::ofstream pcm, trace;
    if (!recording.isEmpty()) {
        if (!QDir().mkpath(recording + "/frames")) return 3;
        pcm.open((recording + "/synthetic.f32").toStdString(), std::ios::binary);
        trace.open((recording + "/trace.csv").toStdString());
        if (!pcm || !trace) return 3;
    }
    if (recording.isEmpty() && !option("--trace").isEmpty()) {
        trace.open(option("--trace").toStdString());
        if (!trace) return 3;
    }
    if (trace.is_open()) trace << "time,energy,bass,mid,treble,phase,arch,counterBend,bias,opening\n";
    const auto publish = [&] {
        view.rootObject()->setProperty("frame", study.frame());
        const auto &s = study.snapshot;
        view.rootObject()->setProperty("stage", synthetic ? section(s.time)
            : s.status.device + QStringLiteral(" · ") + s.status.message);
        view.rootObject()->setProperty("audioTime", s.time);
        if (trace.is_open() && (!synthetic || study.sampleIndex > 0)) {
            trace << s.time << ',' << s.features.energy << ',' << s.features.bass << ',' << s.features.mid << ','
                << s.features.treble << ',' << s.phase;
            for (const float x : study.motion.value) trace << ',' << x;
            trace << '\n';
        }
    };
    publish();
    int frameNumber = 0;
    std::function<void()> nextFrame;
    nextFrame = [&] {
        if (frameNumber == int(Duration * 30)) {
            std::cout << "Recorded " << frameNumber << " frames. Final energy=" << study.snapshot.features.energy << '\n';
            app.quit();
            return;
        }
        for (unsigned i = 0; i < 4; ++i) study.advanceSynthetic(400, &pcm);
        publish();
        // One pending capture at a time. Slow rendering cannot build a frame queue.
        QTimer::singleShot(35, &app, [&] {
            const QString file = recording + QString("/frames/%1.png").arg(frameNumber++, 4, 10, QChar('0'));
            if (!view.grabWindow().save(file)) { app.exit(3); return; }
            nextFrame();
        });
    };
    QElapsedTimer elapsed;
    elapsed.start();
    QTimer analysisTimer, displayTimer;
    QObject::connect(&analysisTimer, &QTimer::timeout, &app, [&] {
        const float dt = float(elapsed.restart()) / 1000;
        if (synthetic) {
            // Replay only after the study's four-second silence has faded out.
            if (study.sampleIndex >= uint64_t(Duration * 48000)) {
                study.analyzer.reset();
                study.motion = {};
                study.snapshot = {};
                study.sampleIndex = 0;
            }
            study.advanceSynthetic(480);
        }
        else study.advanceLive(dt);
    });
    QObject::connect(&displayTimer, &QTimer::timeout, &app, [&] {
        if (view.isVisible() && view.visibility() != QWindow::Minimized) publish();
    });
    if (!recording.isEmpty()) QTimer::singleShot(250, &app, nextFrame);
    else if (!frozen) {
        analysisTimer.setTimerType(Qt::PreciseTimer);
        analysisTimer.start(10);
        displayTimer.start(34);
    }
    if (!capture.isEmpty()) QTimer::singleShot(frozen ? 400 : 5000, &app, [&] {
        const bool saved = view.grabWindow().save(capture);
        std::cout << "capture=" << saved << " device=" << study.snapshot.status.device.toStdString()
            << " energy=" << study.snapshot.features.energy << " dropped=" << study.snapshot.dropped
            << " expired=" << study.snapshot.expired << " shape=";
        for (const float x : study.motion.value) std::cout << x << ' ';
        std::cout << '\n';
        app.exit(saved ? 0 : 3);
    });
    return app.exec();
}
