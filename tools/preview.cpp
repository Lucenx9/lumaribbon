// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioState.h"
#include "SignalAnalyzer.h"
#include "RibbonMotion.h"
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QTimer>
#include <QImage>
#include <cmath>
#include <iostream>

// Only the explicitly requested --synthetic preview generates samples.
// The applet itself always uses the native PipeWire monitor.
class SyntheticAudio : public QObject {
    Q_OBJECT
public:
    explicit SyntheticAudio(int fixedSteps = -1) {
        QObject::connect(&timer, &QTimer::timeout, this, &SyntheticAudio::advance);
        if (fixedSteps >= 0) {
            for (int i = 0; i < fixedSteps; ++i) advance();
        } else {
            timer.start(10);
        }
    }
    Q_INVOKABLE QVariantMap sample(double sensitivity) const {
        auto f = analyzer.features();
        const auto scale = [sensitivity](float x) { return std::clamp(x * sensitivity, 0.0, 1.0); };
        return {{"energy", scale(f.energy)}, {"bass", scale(f.bass)}, {"mid", scale(f.mid)},
            {"treble", scale(f.treble)}, {"onset", scale(f.onset)}, {"phase", phase}, {"rippleAge", f.rippleAge},
            {"rippleOrigin", f.rippleOrigin},
            {"spectralBalance", f.spectralBalance}, {"trebleShare", f.trebleShare},
            {"arch", shape.arch}, {"counterBend", shape.counterBend}, {"bias", shape.bias}, {"opening", shape.opening},
            {"lift", shape.lift}, {"lean", shape.lean},
            {"bassAccent", scale(f.accents[0])}, {"midAccent", scale(f.accents[1])}, {"trebleAccent", scale(f.accents[2])}};
    }
private:
    void advance() {
        std::array<Luma::StereoFrame, 480> block;
        for (auto &frame : block) {
            const double t = double(sampleIndex++) / 48000;
            const double pulse = std::exp(-std::fmod(t, 0.72) * 15);
            const float sample = float((0.075 + pulse * 0.15) * std::sin(t * 6.2831853 * 94)
                + 0.048 * (0.6 + 0.4 * std::sin(t * 1.2)) * std::sin(t * 6.2831853 * 740)
                + pulse * 0.035 * std::sin(t * 6.2831853 * 4800));
            frame = {sample, sample};
        }
        analyzer.feed(block, 48000);
        const auto next = analyzer.features();
        shape = motion.advance(next, 0.01f);
        phase += 0.01 * (0.08 + 0.55 * next.energy + 0.65 * next.mid);
    }
    Luma::SignalAnalyzer analyzer;
    Luma::RibbonMotion motion;
    Luma::RibbonShape shape;
    QTimer timer;
    uint64_t sampleIndex = 0;
    double phase = 0;
};

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    const auto args = app.arguments();
    const bool synthetic = args.contains("--synthetic");
    int fixedSteps = -1;
    const int atIndex = args.indexOf("--at");
    if (atIndex >= 0) {
        bool valid = false;
        const double seconds = args.value(atIndex + 1).toDouble(&valid);
        if (!synthetic || !valid || !std::isfinite(seconds) || seconds < 0 || seconds > 30) {
            std::cerr << "--at requires --synthetic and a time between 0 and 30 seconds.\n";
            return 1;
        }
        fixedSteps = int(std::round(seconds * 100));
    }
    std::unique_ptr<QObject> audio = synthetic ? std::unique_ptr<QObject>(new SyntheticAudio(fixedSteps)) : std::unique_ptr<QObject>(new AudioState);
    QQuickView view;
    view.setTitle(synthetic ? "Luma Ribbon · synthetic signal check" : "Luma Ribbon · PipeWire monitor");
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setInitialProperties({{"previewAudio", QVariant::fromValue(audio.get())}, {"syntheticInput", synthetic},
        {"simpleRendering", args.contains("--fallback")}, {"reducedPreview", args.contains("--reduced")},
        {"referenceTime", fixedSteps >= 0 ? fixedSteps / 100.0 : -1.0}});
    view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/tools/Preview.qml")));
    if (view.status() == QQuickView::Error) return 1;
    view.resize(860, 720);
    view.show();
    const int captureIndex = args.indexOf("--capture");
    if (captureIndex >= 0 && captureIndex + 1 < args.size()) {
        QTimer::singleShot(2600, &app, [&] {
            const QImage capture = view.grabWindow();
            const bool ok = !capture.isNull() && capture.save(args[captureIndex + 1]);
            std::cout << "capture=" << ok << " size=" << capture.width() << "x" << capture.height() << '\n';
            app.exit(ok ? 0 : 2);
        });
    }
    return app.exec();
}
#include "preview.moc"
