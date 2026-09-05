// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QTest>
#include <QSignalSpy>
#include <QImage>
#include <QDir>
#include <QTransform>
#include <QVariantMap>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include "SignalAnalyzer.h"
#include "RibbonMotion.h"

class FrameAudio : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current MEMBER current)
public:
    QVariantMap current;
    int samples = 0;
    Q_INVOKABLE QVariantMap sample(double) { ++samples; return current; }
};

namespace {
QVariantMap analyzedMix(bool includeMids) {
    Luma::SignalAnalyzer analyzer;
    Luma::RibbonMotion motion;
    Luma::RibbonShape shape;
    std::array<Luma::StereoFrame, 480> block;
    unsigned index = 0;
    for (unsigned tick = 0; tick < 600; ++tick) {
        for (auto &frame : block) {
            const double t = double(index++) / 48000;
            const float sample = 0.10 * std::sin(2 * std::numbers::pi * 94 * t)
                + (includeMids ? 0.08 : 0.0) * std::sin(2 * std::numbers::pi * 740 * t)
                + 0.10 * std::sin(2 * std::numbers::pi * 4800 * t);
            frame = {sample, -sample};
        }
        analyzer.feed(block, 48000);
        shape = motion.advance(analyzer.features(), 0.01f);
    }
    const auto f = analyzer.features();
    return {{"energy", f.energy}, {"bass", f.bass}, {"mid", f.mid}, {"treble", f.treble},
        {"phase", 0.65}, {"onset", 0.0}, {"rippleAge", 10.0}, {"rippleOrigin", 0.5},
        {"bassAccent", 0.0}, {"midAccent", 0.0}, {"trebleAccent", 0.0},
        {"arch", shape.arch}, {"counterBend", shape.counterBend}, {"bias", shape.bias}, {"opening", shape.opening}};
}

QVariantMap restingFrame() {
    // Hold the slow shape, levels, color and shared ripple fixed. Only a band's
    // independent transient can change the captured image in these fixtures.
    return {{"energy", 0.65}, {"bass", 0.4}, {"mid", 0.5}, {"treble", 0.3},
        {"phase", 0.65}, {"onset", 0.0}, {"rippleAge", 10.0}, {"rippleOrigin", 0.5},
        {"bassAccent", 0.0}, {"midAccent", 0.0}, {"trebleAccent", 0.0},
        {"arch", 0.25}, {"counterBend", 0.45}, {"bias", -0.12}, {"opening", 0.6}};
}

QImage capture(QQuickView &view, const QVariantMap &frame) {
    view.rootObject()->property("audio").value<QObject *>()->setProperty("current", frame);
    QMetaObject::invokeMethod(view.rootObject(), "refresh");
    QTest::qWait(60);
    return view.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied);
}

uint64_t light(const QImage &image) {
    uint64_t result = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            for (int c = 0; c < 3; ++c) result += image.constScanLine(y)[x * 4 + c];
    return result;
}

unsigned changedChannels(const QImage &a, const QImage &b) {
    unsigned changed = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            for (int c = 0; c < 3; ++c)
                changed += std::abs(a.constScanLine(y)[x * 4 + c] - b.constScanLine(y)[x * 4 + c]) > 2;
    return changed;
}

double center(const QImage &image, int x) {
    double mass = 0, moment = 0;
    for (int y = 0; y < image.height(); ++y) {
        const auto alpha = image.constScanLine(y)[x * 4 + 3];
        mass += alpha;
        moment += alpha * y;
    }
    return mass > 0 ? moment / mass : 0;
}

double excursion(const QImage &image) {
    double low = image.height(), high = 0;
    for (int x = image.width() / 8; x < image.width() * 7 / 8; ++x) {
        low = std::min(low, center(image, x));
        high = std::max(high, center(image, x));
    }
    return (high - low) / image.height();
}

double spread(const QImage &image) {
    double mass = 0, variance = 0;
    for (int x = image.width() / 4; x < image.width() * 3 / 4; ++x) {
        const double mean = center(image, x);
        for (int y = 0; y < image.height(); ++y) {
            const auto alpha = image.constScanLine(y)[x * 4 + 3];
            mass += alpha;
            variance += alpha * (y - mean) * (y - mean);
        }
    }
    return std::sqrt(variance / std::max(1.0, mass)) / image.height();
}
}

class MotionViewTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void bloomControl_data() {
        QTest::addColumn<bool>("fallback");
        QTest::addColumn<QSize>("size");
        QTest::addColumn<bool>("lightBackground");
        for (bool fallback : {false, true})
            for (const auto &size : {QSize(160, 32), QSize(200, 40), QSize(240, 48), QSize(40, 200), QSize(560, 260)})
                for (bool lightBackground : {false, true})
                    QTest::newRow(qPrintable(QString("%1-%2x%3-%4").arg(fallback ? "canvas" : "shader")
                        .arg(size.width()).arg(size.height()).arg(lightBackground ? "light" : "dark")))
                        << fallback << size << lightBackground;
    }
    void bloomControl() {
        QFETCH(bool, fallback);
        QFETCH(QSize, size);
        QFETCH(bool, lightBackground);
        FrameAudio audio;
        auto frame = analyzedMix(true);
        for (const auto *key : {"energy", "bass", "mid", "treble"})
            frame[key] = std::min(1.0, frame[key].toDouble() * 2.0);
        for (const auto *key : {"bassAccent", "midAccent", "trebleAccent", "onset"}) frame[key] = 1.0;
        frame["rippleAge"] = 0.12;
        audio.current = frame;
        const bool vertical = size.height() > size.width();
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false},
            {"forceFallback", fallback}, {"dynamicColor", false}, {"paletteIndex", 1}, {"vertical", vertical},
            {"intensity", 1.6}, {"curvature", 1.25}, {"fullness", 1.3},
            {"backdropColor", lightBackground ? "#ffffff" : "#161824"}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(size);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const auto original = capture(view, frame);
        const auto shape = view.rootObject()->property("shape");
        const int samples = audio.samples;
        const auto setBloom = [&](double amount) {
            view.rootObject()->setProperty("bloom", amount);
            QTest::qWait(65);
            return view.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        };
        const auto off = setBloom(0.0);
        const auto half = setBloom(0.5);
        const auto maximum = setBloom(1.5);
        QVERIFY(!original.isNull() && !off.isNull() && !maximum.isNull());
        QVERIFY(!view.rootObject()->property("shaderFailed").toBool());
        QVERIFY2(light(off) > 1000, "Disabling bloom must keep the filaments visible.");
        QVERIFY(light(half) > light(off));
        QVERIFY(light(original) > light(half));
        QVERIFY(light(maximum) > light(original));
        QCOMPARE(view.rootObject()->property("shape"), shape);
        QCOMPARE(audio.samples, samples); // Repaint a held frame without touching analysis.
        const auto edges = vertical ? maximum.transformed(QTransform().rotate(90)) : maximum;
        for (int x = 0; x < edges.width(); ++x) {
            QVERIFY(edges.pixelColor(x, 0).alpha() < 3);
            QVERIFY(edges.pixelColor(x, edges.height() - 1).alpha() < 3);
        }
        QCOMPARE(setBloom(1.0), original);
        QCOMPARE(setBloom(-100.0), off);
        QCOMPARE(setBloom(100.0), maximum);
        QCOMPARE(setBloom(std::numeric_limits<double>::quiet_NaN()), original);
        QCOMPARE(setBloom(std::numeric_limits<double>::infinity()), original);
        const auto directory = qEnvironmentVariable("LUMA_BLOOM_CAPTURE");
        if (!directory.isEmpty()) {
            const QDir dir(directory);
            QVERIFY(off.save(dir.filePath(QString("%1-off.png").arg(QTest::currentDataTag()))));
            QVERIFY(original.save(dir.filePath(QString("%1-default.png").arg(QTest::currentDataTag()))));
            QVERIFY(maximum.save(dir.filePath(QString("%1-max.png").arg(QTest::currentDataTag()))));
        }
        view.rootObject()->setProperty("bloom", 1.5);
        view.rootObject()->setProperty("reducedMotion", true);
        const auto reduced = capture(view, frame);
        frame["phase"] = 14.0;
        frame["arch"] = -0.8;
        QCOMPARE(capture(view, frame), reduced);
        frame["energy"] = 0.0;
        QCOMPARE(light(capture(view, frame)), uint64_t(0));
    }
    void appearanceControls_data() {
        QTest::addColumn<bool>("fallback");
        QTest::addColumn<QSize>("size");
        for (bool fallback : {false, true})
            for (const auto &size : {QSize(160, 32), QSize(200, 40), QSize(240, 48), QSize(40, 200), QSize(560, 260)})
                QTest::newRow(qPrintable(QString("%1-%2x%3").arg(fallback ? "canvas" : "shader").arg(size.width()).arg(size.height())))
                    << fallback << size;
    }
    void appearanceControls() {
        QFETCH(bool, fallback);
        QFETCH(QSize, size);
        FrameAudio audio;
        auto frame = analyzedMix(true);
        for (const auto *key : {"energy", "bass", "mid", "treble"})
            frame[key] = std::min(1.0, frame[key].toDouble() * 2.0);
        for (const auto *key : {"bassAccent", "midAccent", "trebleAccent", "onset"}) frame[key] = 1.0;
        frame["rippleAge"] = 0.12;
        audio.current = frame;
        const bool vertical = size.height() > size.width();
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false},
            {"forceFallback", fallback}, {"dynamicColor", false}, {"paletteIndex", 1}, {"vertical", vertical}, {"intensity", 1.6}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(size);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const auto baseline = capture(view, frame);
        const auto grab = [&]() {
            QTest::qWait(65);
            auto image = view.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            return vertical ? image.transformed(QTransform().rotate(90)) : image;
        };
        QImage corners[2][2];
        const int samples = audio.samples;
        for (int c = 0; c < 2; ++c) {
            for (int f = 0; f < 2; ++f) {
                view.rootObject()->setProperty("curvature", c ? 1.25 : 0.5);
                view.rootObject()->setProperty("fullness", f ? 1.3 : 0.6);
                auto &image = corners[c][f];
                image = grab(); // No frame refresh: draft edits must repaint on their own.
                QVERIFY(!image.isNull() && light(image) > 1000);
                QVERIFY(!view.rootObject()->property("shaderFailed").toBool());
                QVERIFY(excursion(image) < 0.55);
                for (int x = 0; x < image.width(); ++x) {
                    QVERIFY2(image.pixelColor(x, 0).alpha() < 3, "Appearance bounds must not clip the top halo.");
                    QVERIFY2(image.pixelColor(x, image.height() - 1).alpha() < 3, "Appearance bounds must not clip the bottom halo.");
                }
                const auto directory = qEnvironmentVariable("LUMA_APPEARANCE_CAPTURE");
                if (!directory.isEmpty())
                    QVERIFY(image.save(QDir(directory).filePath(QString("%1-c%2-f%3.png").arg(QTest::currentDataTag()).arg(c).arg(f))));
            }
        }
        QCOMPARE(audio.samples, samples);
        for (int f = 0; f < 2; ++f)
            QVERIFY2(excursion(corners[1][f]) > excursion(corners[0][f]) * 1.5, "Curvature must change the broad curve.");
        for (int c = 0; c < 2; ++c)
            QVERIFY2(spread(corners[c][1]) > spread(corners[c][0]) * 1.3, "Fullness must widen the bundle around its center.");
        // Returning to the defaults is exact; invalid config values stay finite.
        view.rootObject()->setProperty("curvature", 1.0);
        view.rootObject()->setProperty("fullness", 1.0);
        const auto defaults = grab();
        QCOMPARE(defaults, vertical ? baseline.transformed(QTransform().rotate(90)) : baseline);
        view.rootObject()->setProperty("curvature", std::numeric_limits<double>::quiet_NaN());
        view.rootObject()->setProperty("fullness", std::numeric_limits<double>::infinity());
        QCOMPARE(grab(), defaults);
        view.rootObject()->setProperty("curvature", -500.0);
        view.rootObject()->setProperty("fullness", 500.0);
        QCOMPARE(grab(), corners[0][1]);
        view.rootObject()->setProperty("curvature", 1.25);
        view.rootObject()->setProperty("reducedMotion", true);
        const auto reduced = capture(view, frame);
        frame["phase"] = 17.0;
        frame["arch"] = -0.8;
        QCOMPARE(capture(view, frame), reduced);
        frame["energy"] = 0.0;
        QCOMPARE(light(capture(view, frame)), uint64_t(0));
    }
    void mixedBandBody_data() {
        QTest::addColumn<bool>("fallback");
        QTest::addColumn<bool>("includeMids");
        QTest::addColumn<QSize>("size");
        QTest::addColumn<double>("gain");
        for (bool fallback : {false, true})
            for (bool includeMids : {false, true})
                for (const auto &size : {QSize(160, 32), QSize(200, 40), QSize(240, 48), QSize(40, 200)})
                    for (double gain : {1.0, 1.5})
                        QTest::newRow(qPrintable(QString("%1-%2-%3x%4-gain%5").arg(fallback ? "canvas" : "shader")
                            .arg(includeMids ? "full-mix" : "bass-highs").arg(size.width()).arg(size.height()).arg(gain)))
                            << fallback << includeMids << size << gain;
    }
    void mixedBandBody() {
        QFETCH(bool, fallback);
        QFETCH(bool, includeMids);
        QFETCH(QSize, size);
        QFETCH(double, gain);
        FrameAudio audio;
        audio.current = analyzedMix(includeMids);
        // Sensitivity scales levels after analysis; the shared shape is unchanged.
        for (const auto *key : {"energy", "bass", "mid", "treble"})
            audio.current[key] = std::min(1.0, audio.current[key].toDouble() * gain);
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        const bool vertical = size.height() > size.width();
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false},
            {"forceFallback", fallback}, {"dynamicColor", false}, {"paletteIndex", 1}, {"vertical", vertical},
            {"intensity", gain}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(size);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto image = capture(view, audio.current);
        QVERIFY(!image.isNull());
        QVERIFY(!view.rootObject()->property("shaderFailed").toBool());
        if (vertical) image = image.transformed(QTransform().rotate(90));
        double low = image.height(), high = 0;
        for (int x = image.width() / 8; x < image.width() * 7 / 8; ++x) {
            low = std::min(low, center(image, x));
            high = std::max(high, center(image, x));
        }
        const double excursion = (high - low) / image.height();
        qInfo() << "Mixed-band vertical excursion" << excursion << "shape"
            << audio.current["arch"] << audio.current["counterBend"] << audio.current["opening"];
        QVERIFY2(excursion > 0.12, "An audible multi-band mix must retain a readable broad curve at panel size.");
        QVERIFY2(excursion < 0.42, "The broad curve must leave room for filaments and attacks.");
        for (int x = 0; x < image.width(); ++x) {
            QVERIFY2(image.pixelColor(x, 0).alpha() < 3, "The upper halo must fade before the panel clips it.");
            QVERIFY2(image.pixelColor(x, image.height() - 1).alpha() < 3, "The lower halo must fade before the panel clips it.");
        }
    }

    void sharedShape_data() {
        QTest::addColumn<bool>("fallback");
        QTest::newRow("shader") << false;
        QTest::newRow("canvas") << true;
    }
    void sharedShape() {
        QFETCH(bool, fallback);
        FrameAudio audio;
        audio.current = restingFrame();
        QQuickView panel, popup;
        for (auto *view : {&panel, &popup}) {
            view->setColor(Qt::transparent);
            view->setResizeMode(QQuickView::SizeRootObjectToView);
            view->setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false},
                {"forceFallback", fallback}, {"dynamicColor", false}});
            view->setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
            QCOMPARE(view->status(), QQuickView::Ready);
            view->resize(200, 40);
        }
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        auto frame = audio.current;
        const auto first = capture(panel, frame);
        // Only the slow shape changes: phase, levels, transients and colors stay fixed.
        frame["arch"] = -0.6;
        frame["counterBend"] = -0.15;
        frame["bias"] = 0.5;
        frame["opening"] = 0.3;
        const auto changed = capture(panel, frame);
        QVERIFY(changedChannels(first, changed) > 400);
        double displacement = 0;
        for (int x = changed.width() / 5; x < changed.width() * 4 / 5; ++x)
            displacement = std::max(displacement, std::abs(center(changed, x) - center(first, x)));
        QVERIFY(displacement > 3 * panel.devicePixelRatio());
        // Reopening gets the latest shape immediately, with no per-view catch-up.
        popup.rootObject()->setProperty("fps", 60);
        popup.rootObject()->setProperty("viewEnabled", true);
        popup.show();
        QVERIFY(QTest::qWaitForWindowExposed(&popup));
        QTest::qWait(80);
        QCOMPARE(popup.rootObject()->property("shape"), panel.rootObject()->property("shape"));
        QCOMPARE(popup.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied), changed);
        popup.hide();
        QTest::qWait(30);
        QSignalSpy changes(popup.rootObject(), SIGNAL(frameChanged()));
        frame["arch"] = 0.7;
        const auto latest = capture(panel, frame);
        QTest::qWait(80);
        QCOMPARE(changes.count(), 0);
        popup.show();
        QVERIFY(QTest::qWaitForWindowExposed(&popup));
        QTest::qWait(80);
        QCOMPARE(popup.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied), latest);
        // Reduced motion fixes broad shape as well as phase and short attacks.
        panel.rootObject()->setProperty("reducedMotion", true);
        const auto reduced = capture(panel, frame);
        frame["arch"] = -0.8;
        frame["counterBend"] = 1.0;
        frame["bias"] = -0.6;
        frame["opening"] = 0.9;
        frame["phase"] = 15.0;
        QCOMPARE(capture(panel, frame), reduced);
    }
    void independentMidAccent_data() {
        QTest::addColumn<QSize>("size");
        QTest::addColumn<bool>("vertical");
        QTest::addColumn<bool>("fallback");
        for (bool fallback : {false, true}) {
            const char *renderer = fallback ? "canvas" : "shader";
            for (const auto &size : {QSize(160, 32), QSize(200, 40), QSize(240, 48), QSize(432, 180)})
                QTest::newRow(qPrintable(QString("%1-%2x%3").arg(renderer).arg(size.width()).arg(size.height())))
                    << size << false << fallback;
            QTest::newRow(qPrintable(QString("%1-vertical").arg(renderer))) << QSize(40, 200) << true << fallback;
        }
    }

    void independentMidAccent() {
        QFETCH(QSize, size);
        QFETCH(bool, vertical);
        QFETCH(bool, fallback);
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        auto frame = restingFrame();
        FrameAudio audio;
        audio.current = frame;
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false}, {"dynamicColor", false},
            {"forceFallback", fallback}, {"vertical", vertical}, {"frame", frame}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(size);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY(!view.rootObject()->property("shaderFailed").toBool());

        const auto resting = capture(view, frame);
        QVERIFY(!resting.isNull());
        frame["midAccent"] = 0.8;
        const auto accented = capture(view, frame);
        QCOMPARE(accented.size(), resting.size());
        const unsigned changed = changedChannels(resting, accented);
        qInfo() << "mid changed channels" << changed;
        QVERIFY2(changed > 80, "Mid accents must remain visible with no shared ripple and a fixed slow shape.");

        auto a = resting, b = accented;
        if (vertical) {
            a = a.transformed(QTransform().rotate(90));
            b = b.transformed(QTransform().rotate(90));
        }
        const double shortSide = vertical ? size.width() : size.height();
        const double scale = a.height() / shortSide;
        double peak = 0;
        for (int x = a.width() / 10; x < a.width() * 9 / 10; ++x)
            peak = std::max(peak, std::abs(center(b, x) - center(a, x)) / scale);
        const double lightRatio = double(light(accented)) / double(light(resting));
        qInfo() << "mid displacement px" << peak << "light ratio" << lightRatio;
        QVERIFY2(peak > shortSide * 0.008, "The accent must bend the ribbon, not only change its brightness.");
        QVERIFY2(peak < shortSide * 0.028 + 0.35 / scale, "The fast bend must stay small at panel size.");
        QVERIFY(lightRatio > 0.9 && lightRatio < 1.1);

        frame["midAccent"] = 0.0;
        QCOMPARE(capture(view, frame), resting); // No retained per-view animation state.
        view.rootObject()->setProperty("reducedMotion", true);
        const auto reduced = capture(view, frame);
        frame["midAccent"] = 1.0;
        frame["onset"] = 1.0;
        frame["rippleAge"] = 0.03;
        QCOMPARE(capture(view, frame), reduced);
        frame["phase"] = 12.0;
        QCOMPARE(capture(view, frame), reduced);

        frame["energy"] = 0.0;
        const auto silent = capture(view, frame);
        for (int y = 0; y < silent.height(); ++y)
            for (int x = 0; x < silent.width(); ++x) QCOMPARE(silent.constScanLine(y)[x * 4 + 3], 0);
    }

    void otherAccents_data() {
        QTest::addColumn<QString>("band");
        QTest::addColumn<bool>("fallback");
        for (const auto &band : {QStringLiteral("bassAccent"), QStringLiteral("trebleAccent")})
            for (bool fallback : {false, true})
                QTest::newRow(qPrintable(band + (fallback ? "-canvas" : "-shader"))) << band << fallback;
    }

    void otherAccents() {
        QFETCH(QString, band);
        QFETCH(bool, fallback);
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        auto frame = restingFrame();
        FrameAudio audio;
        audio.current = frame;
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false}, {"dynamicColor", false},
            {"forceFallback", fallback}, {"frame", frame}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(200, 40);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const auto resting = capture(view, frame);
        frame[band] = 0.8;
        const auto accented = capture(view, frame);
        QCOMPARE(accented.size(), resting.size());
        QVERIFY(changedChannels(resting, accented) > 80);
        QVERIFY(double(light(accented)) / double(light(resting)) < 1.3);
    }
};

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    MotionViewTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "test_motion_view.moc"
