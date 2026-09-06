// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QTest>
#include <QSignalSpy>
#include <QVariantMap>
#include <QImage>
#include <QJSValue>
#include <QDir>
#include <QTransform>
#include <numbers>
#include <cmath>
#include <array>
#include <limits>

// Measure the displayed QColor in Oklab; do not reuse the palette generator.
static std::array<double, 3> perceptualColor(QColor color) {
    const auto linear = [](double x) { return x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4); };
    const double r = linear(color.redF()), g = linear(color.greenF()), b = linear(color.blueF());
    const double l = std::cbrt(0.4122214708*r + 0.5363325363*g + 0.0514459929*b);
    const double m = std::cbrt(0.2119034982*r + 0.6806995451*g + 0.1073969566*b);
    const double s = std::cbrt(0.0883024619*r + 0.2817188376*g + 0.6299787005*b);
    return {0.2104542553*l + 0.7936177850*m - 0.0040720468*s,
        1.9779984951*l - 2.4285922050*m + 0.4505937099*s,
        0.0259040371*l + 0.7827717662*m - 0.8086757660*s};
}

// True when the simple renderer is active for reasons other than a real
// shader failure: the ripple tests then measure a renderer that by design
// has no travelling ripple.
static bool softwareRendererActive(QQuickView &view) {
    return view.rootObject()->property("fallback").toBool()
        && !view.rootObject()->property("shaderFailed").toBool();
}

class TestAudio : public QObject {
    Q_OBJECT
public:
    int calls = 0;
    bool silent = false;
    double phaseOverride = -1;
    double lastSensitivity = 0;
    double onset = 0, rippleAge = 10, rippleOrigin = 0.46;
    double spectralBalance = 0.5, trebleShare = 0;
    std::array<double, 3> accents{};
Q_SIGNALS:
    void audioAvailable();
public:
    Q_INVOKABLE QVariantMap sample(double sensitivity) {
        ++calls;
        lastSensitivity = sensitivity;
        return {{"energy", silent ? 0.0 : 0.7}, {"bass", 0.75}, {"mid", 0.4}, {"treble", 0.2},
            {"onset", onset}, {"phase", phaseOverride >= 0 ? phaseOverride : calls * 0.02}, {"rippleAge", rippleAge},
            {"rippleOrigin", rippleOrigin},
            {"spectralBalance", spectralBalance}, {"trebleShare", trebleShare},
            {"arch", 0.25}, {"counterBend", 0.45}, {"bias", -0.12}, {"opening", 0.6},
            {"bassAccent", accents[0]}, {"midAccent", accents[1]}, {"trebleAccent", accents[2]}};
    }
};
class ViewTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void multicolor_data() {
        QTest::addColumn<bool>("fallback");
        QTest::addColumn<bool>("light");
        QTest::addColumn<QSize>("size");
        for (bool fallback : {false, true}) {
            for (bool light : {false, true}) {
                for (const auto &size : {QSize(200, 40), QSize(40, 200), QSize(560, 260)}) {
                    const auto tag = QString("%1-%2-%3x%4").arg(fallback ? "canvas" : "shader")
                        .arg(light ? "light" : "dark").arg(size.width()).arg(size.height());
                    QTest::newRow(qPrintable(tag)) << fallback << light << size;
                }
            }
        }
    }
    void multicolor() {
        QFETCH(bool, fallback);
        QFETCH(bool, light);
        QFETCH(QSize, size);
        TestAudio audio;
        audio.phaseOverride = 0.65;
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false},
            {"paletteIndex", 6}, {"forceFallback", fallback}, {"vertical", size.height() > size.width()},
            {"backdropColor", QColor(light ? "#ffffff" : "#20242c")}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(size);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *root = view.rootObject();
        QVERIFY(root->property("multicolor").toBool());
        const auto stops = root->property("spectrumColors").value<QJSValue>();
        QCOMPARE(stops.property("length").toInt(), 7);
        for (int i = 0; i < 7; ++i) {
            const auto color = stops.property(i).toVariant().value<QColor>();
            QVERIFY(color.isValid());
            QCOMPARE(color.alpha(), 255);
            const auto lab = perceptualColor(color);
            QVERIFY(std::abs(lab[0] - (light ? 0.52 : 0.73)) < 0.002);
            QVERIFY(std::hypot(lab[1], lab[2]) > 0.08);
        }
        QCOMPARE(stops.property(0).toVariant(), stops.property(6).toVariant());
        QSignalSpy stopsChanged(root, SIGNAL(spectrumColorsChanged()));
        const auto capture = [&] {
            QMetaObject::invokeMethod(root, "refresh");
            QTest::qWait(65);
            return view.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        };
        const auto colorSectors = [](const QImage &image) {
            unsigned sectors = 0;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    const auto c = image.pixelColor(x, y);
                    if (c.alpha() > 40 && c.hsvSaturationF() > 0.2)
                        sectors |= 1u << std::min(5, int(c.hsvHueF() * 6));
                }
            }
            return sectors;
        };
        audio.spectralBalance = 0.3;
        const auto bass = capture();
        QVERIFY(!bass.isNull());
        QCOMPARE(colorSectors(bass), 63u); // All six hue sectors occur in one frame.
        const auto directory = qEnvironmentVariable("LUMA_SPECTRUM_CAPTURE");
        if (!directory.isEmpty())
            QVERIFY(bass.save(QDir(directory).filePath(QString("%1.png").arg(QTest::currentDataTag()))));
        audio.spectralBalance = 0.9;
        const auto highs = capture();
        QCOMPARE(colorSectors(highs), 63u); // Audio never narrows it to a single hue.
        QVERIFY(highs != bass);
        for (int y = 0; y < bass.height(); ++y)
            for (int x = 0; x < bass.width(); ++x)
                QCOMPARE(bass.constScanLine(y)[x * 4 + 3], highs.constScanLine(y)[x * 4 + 3]);
        QTest::qWait(100);
        QCOMPARE(capture(), highs); // No autonomous color cycling.
        QCOMPARE(stopsChanged.count(), 0); // No gamut conversion on audio updates.
        root->setProperty("dynamicColor", false);
        const auto fixed = capture();
        audio.spectralBalance = 0.3;
        QCOMPARE(capture(), fixed);
        root->setProperty("hue", 90.0);
        const auto shifted = capture();
        QVERIFY(shifted != fixed);
        QCOMPARE(colorSectors(shifted), 63u);
        QVERIFY(stopsChanged.count() > 0);
        root->setProperty("hue", 0.0);
        QCOMPARE(capture(), fixed);
        root->setProperty("hue", 180.0);
        const auto halfTurn = capture();
        root->setProperty("hue", -180.0);
        QCOMPARE(capture(), halfTurn);
        root->setProperty("reducedMotion", true);
        const auto reduced = capture();
        audio.phaseOverride = 12;
        QCOMPARE(capture(), reduced);
        audio.silent = true;
        const auto silent = capture();
        for (int y = 0; y < silent.height(); ++y)
            for (int x = 0; x < silent.width(); ++x) QCOMPARE(silent.constScanLine(y)[x * 4 + 3], 0);
        QVERIFY(!root->property("shaderFailed").toBool());
    }
    void huePalette_data() { perceptualPalette_data(); }
    void huePalette() {
        QFETCH(int, palette);
        QFETCH(bool, light);
        TestAudio audio;
        QQuickView view;
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false},
            {"paletteIndex", palette}, {"backdropColor", QColor(light ? "#ffffff" : "#20242c")}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        auto *root = view.rootObject();
        const std::array<const char *, 3> keys{"primaryColor", "secondaryColor", "highlightColor"};
        std::array<QColor, 3> original;
        for (size_t i = 0; i < keys.size(); ++i) original[i] = root->property(keys[i]).value<QColor>();
        for (int degrees = -180; degrees <= 180; degrees += 5) {
            root->setProperty("hue", degrees);
            for (size_t i = 0; i < keys.size(); ++i) {
                const auto color = root->property(keys[i]).value<QColor>();
                QVERIFY(color.isValid() && color.alphaF() == 1.0);
                for (double channel : {color.redF(), color.greenF(), color.blueF()})
                    QVERIFY(std::isfinite(channel) && channel >= 0 && channel <= 1);
                const auto base = perceptualColor(original[i]), shifted = perceptualColor(color);
                const double baseChroma = std::hypot(base[1], base[2]);
                const double chroma = std::hypot(shifted[1], shifted[2]);
                QVERIFY2(std::abs(base[0] - shifted[0]) < 0.002, "Hue rotation must preserve Oklab lightness.");
                QVERIFY(chroma <= baseChroma + 0.002);
                QVERIFY2(chroma > baseChroma * 0.2, "Gamut mapping must retain a useful amount of color.");
                const double expected = std::atan2(base[2], base[1]) + degrees * std::numbers::pi / 180;
                const double error = std::remainder(std::atan2(shifted[2], shifted[1]) - expected, 2 * std::numbers::pi);
                QVERIFY2(std::abs(error) < 0.02, "Gamut mapping must preserve the chosen hue angle.");
            }
        }
        std::array<QColor, 3> halfTurn;
        for (size_t i = 0; i < keys.size(); ++i) halfTurn[i] = root->property(keys[i]).value<QColor>();
        for (double degrees : {-180.0, -900.0, 900.0}) {
            root->setProperty("hue", degrees);
            for (size_t i = 0; i < keys.size(); ++i) QCOMPARE(root->property(keys[i]).value<QColor>(), halfTurn[i]);
        }
        for (double degrees : {0.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
            root->setProperty("hue", degrees);
            for (size_t i = 0; i < keys.size(); ++i) QCOMPARE(root->property(keys[i]).value<QColor>(), original[i]);
        }
        root->setProperty("hue", 90.0);
        QSignalSpy rampChanges(root, SIGNAL(paletteRampChanged()));
        for (int i = 0; i < 30; ++i) {
            audio.spectralBalance = 0.3 + 0.6 * i / 29;
            QMetaObject::invokeMethod(root, "refresh");
        }
        QCOMPARE(rampChanges.count(), 0); // Rotation and gamut mapping stay outside audio-frame updates.
        root->setProperty("hue", 45.0);
        QVERIFY(rampChanges.count() > 0);
        root->setProperty("dynamicColor", false);
        QCOMPARE(root->property("startColor"), root->property("primaryColor"));
        QCOMPARE(root->property("endColor"), root->property("secondaryColor"));
    }
    void hueRendering_data() { settingsRendering_data(); }
    void hueRendering() {
        QFETCH(int, palette);
        QFETCH(bool, fallback);
        TestAudio audio;
        audio.phaseOverride = 0.65;
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        const QSize size = palette % 3 == 0 ? QSize(560, 260) : palette % 3 == 1 ? QSize(200, 40) : QSize(40, 200);
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false},
            {"paletteIndex", palette}, {"forceFallback", fallback}, {"vertical", size.height() > size.width()}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(size);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *root = view.rootObject();
        const auto grab = [&]() { QTest::qWait(65); return view.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied); };
        for (bool light : {false, true}) {
            root->setProperty("backdropColor", QColor(light ? "#ffffff" : "#20242c"));
            audio.spectralBalance = 0.35;
            root->setProperty("hue", 0.0);
            QMetaObject::invokeMethod(root, "refresh");
            const auto original = grab();
            QSignalSpy frameChanges(root, SIGNAL(frameChanged()));
            const int calls = audio.calls;
            for (double degrees : {-90.0, 90.0, 180.0}) {
                root->setProperty("hue", degrees);
                const auto shifted = grab();
                QVERIFY(!shifted.isNull());
                unsigned changed = 0;
                for (int y = 0; y < shifted.height(); ++y) {
                    for (int x = 0; x < shifted.width(); ++x) {
                        const auto *a = original.constScanLine(y) + 4 * x;
                        const auto *b = shifted.constScanLine(y) + 4 * x;
                        QCOMPARE(a[3], b[3]); // Hue changes color, never geometry or fade.
                        for (int c = 0; c < 3; ++c) changed += std::abs(a[c] - b[c]) > 3;
                    }
                }
                QVERIFY(changed > 100);
                const auto directory = qEnvironmentVariable("LUMA_HUE_CAPTURE");
                if (!directory.isEmpty()) {
                    const auto name = QString("%1-%2-%3.png").arg(QTest::currentDataTag()).arg(light ? "light" : "dark").arg(degrees);
                    QVERIFY(shifted.save(QDir(directory).filePath(name)));
                    QVERIFY(original.save(QDir(directory).filePath(QString("%1-%2-0.png").arg(QTest::currentDataTag()).arg(light ? "light" : "dark"))));
                }
            }
            QCOMPARE(audio.calls, calls);
            QCOMPARE(frameChanges.count(), 0);
            QVERIFY(!root->property("shaderFailed").toBool());
            const auto halfTurn = grab();
            root->setProperty("hue", -180.0);
            QCOMPARE(grab(), halfTurn);
            root->setProperty("hue", 0.0);
            QCOMPARE(grab(), original);
            root->setProperty("hue", 90.0);
            const auto low = grab();
            audio.spectralBalance = 0.85;
            QMetaObject::invokeMethod(root, "refresh");
            QVERIFY(grab() != low); // Audio-reactive color still works after rotation.
        }
        audio.silent = true;
        QMetaObject::invokeMethod(root, "refresh");
        const auto silent = grab();
        for (int y = 0; y < silent.height(); ++y)
            for (int x = 0; x < silent.width(); ++x) QCOMPARE(silent.constScanLine(y)[x * 4 + 3], 0);
    }
    void wakeFromSilence() {
        TestAudio audio;
        audio.silent = true;
        QQuickView panel, popup;
        for (auto *view : {&panel, &popup}) {
            view->setResizeMode(QQuickView::SizeRootObjectToView);
            view->setInitialProperties({{"audio", QVariant::fromValue(&audio)}});
            view->setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
            QCOMPARE(view->status(), QQuickView::Ready);
            view->resize(200, 40);
            view->show();
            QVERIFY(QTest::qWaitForWindowExposed(view));
        }
        QSignalSpy panelChanges(panel.rootObject(), SIGNAL(frameChanged()));
        QSignalSpy popupChanges(popup.rootObject(), SIGNAL(frameChanged()));
        QTest::qWait(150);
        QCOMPARE(panelChanges.count(), 0);
        QCOMPARE(popupChanges.count(), 0);
        audio.silent = false;
        Q_EMIT audio.audioAvailable();
        // Both views read the latest snapshot in the notification's GUI turn.
        QCOMPARE(panelChanges.count(), 1);
        QCOMPARE(popupChanges.count(), 1);
        panel.hide();
        popup.hide();
        QTest::qWait(30);
        const int calls = audio.calls;
        Q_EMIT audio.audioAvailable();
        QCOMPARE(audio.calls, calls);
    }
    void perceptualPalette_data() {
        QTest::addColumn<int>("palette");
        QTest::addColumn<bool>("light");
        for (int palette = 0; palette < 6; ++palette) {
            QTest::newRow(qPrintable(QStringLiteral("palette-%1-dark").arg(palette))) << palette << false;
            QTest::newRow(qPrintable(QStringLiteral("palette-%1-light").arg(palette))) << palette << true;
        }
    }
    void perceptualPalette() {
        QFETCH(int, palette);
        QFETCH(bool, light);
        TestAudio audio;
        QQuickView view;
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false},
            {"paletteIndex", palette}, {"backdropColor", QColor(light ? "#ebedf0" : "#20242c")}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        auto *root = view.rootObject();
        const auto ramp = root->property("paletteRamp").value<QJSValue>();
        QCOMPARE(ramp.property("length").toInt(), 65);
        QSignalSpy rampChanges(root, SIGNAL(paletteRampChanged()));
        const auto a = perceptualColor(root->property("primaryColor").value<QColor>());
        const auto b = perceptualColor(root->property("secondaryColor").value<QColor>());
        const double chromaA = std::hypot(a[1], a[2]), chromaB = std::hypot(b[1], b[2]);
        std::array<double, 3> previous{};
        double minChromaFraction = 1, maxStep = 0;
        for (unsigned i = 0; i <= 300; ++i) {
            audio.spectralBalance = 0.3 + 0.6 * i / 300;
            QMetaObject::invokeMethod(root, "refresh");
            const auto color = root->property("startColor").value<QColor>();
            QVERIFY(color.isValid());
            QCOMPARE(color.alphaF(), 1);
            const auto lab = perceptualColor(color);
            // Check interpolation at the selected endpoint, including the
            // wider gradient. Keep the independent Oklab conversion below.
            const double t = 0.84 * root->property("colorBalance").toDouble()
                - root->property("colorSpread").toDouble();
            const double expectedL = a[0] + (b[0] - a[0]) * t;
            QVERIFY2(std::abs(lab[0] - expectedL) < 0.002, "A palette transition dips or overshoots in perceived lightness.");
            const double expectedChroma = chromaA + (chromaB - chromaA) * t;
            minChromaFraction = std::min(minChromaFraction, std::hypot(lab[1], lab[2]) / expectedChroma);
            if (i) maxStep = std::max(maxStep, std::sqrt(std::pow(lab[0]-previous[0], 2)
                + std::pow(lab[1]-previous[1], 2) + std::pow(lab[2]-previous[2], 2)));
            previous = lab;
        }
        qInfo() << "Minimum retained chroma fraction" << minChromaFraction << "maximum perceptual step" << maxStep;
        QVERIFY2(minChromaFraction > 0.65, "Intermediate hues become too grey, even allowing for sRGB gamut limits.");
        QVERIFY2(maxStep < 0.008, "Palette lookup or gamut mapping creates a visible color step.");
        root->setProperty("dynamicColor", false);
        QCOMPARE(root->property("startColor"), root->property("primaryColor"));
        QCOMPARE(root->property("endColor"), root->property("secondaryColor"));
        QCOMPARE(rampChanges.count(), 0); // Audio frames never rebuild the color table.
        root->setProperty("paletteIndex", (palette + 1) % 6);
        QVERIFY(rampChanges.count() > 0);
    }
    void dynamicColors_data() { settingsRendering_data(); }
    void dynamicColors() {
        QFETCH(int, palette);
        QFETCH(bool, fallback);
        TestAudio audio;
        audio.phaseOverride = 0.65;
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false},
            {"paletteIndex", palette}, {"forceFallback", fallback}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(200, 40);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *root = view.rootObject();
        QVERIFY(root->property("dynamicColor").toBool());
        const auto capture = [&] {
            QMetaObject::invokeMethod(root, "refresh");
            QTest::qWait(60);
            return view.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        };
        for (const auto *backdrop : {"#20242c", "#ebedf0"}) {
            root->setProperty("backdropColor", QColor(backdrop));
            audio.spectralBalance = 0.02;
            audio.trebleShare = 0;
            const QImage bass = capture();
            QVERIFY(!bass.isNull());
            audio.spectralBalance = 0.65;
            const QImage mids = capture();
            audio.spectralBalance = 0.98;
            audio.trebleShare = 0.9;
            const QImage highs = capture();
            QVERIFY(mids != bass && highs != mids);
            double difference = 0;
            unsigned visiblePixels = 0;
            for (int y = 0; y < bass.height(); ++y) {
                for (int x = 0; x < bass.width(); ++x) {
                    const QColor low = bass.pixelColor(x, y), high = highs.pixelColor(x, y);
                    QCOMPARE(low.alpha(), high.alpha()); // Color must not pump the global opacity.
                    if (low.alpha() > 30) {
                        difference += std::abs(low.red() - high.red()) + std::abs(low.green() - high.green())
                            + std::abs(low.blue() - high.blue());
                        ++visiblePixels;
                    }
                }
            }
            QVERIFY(visiblePixels > 100);
            qInfo() << "Mean color change per visible channel" << difference / (3 * visiblePixels);
            QVERIFY2(difference / (3 * visiblePixels) > 12, "Audio-driven hue changes are too subtle at panel size.");
            // Smaller shifts typical of a full mix must also remain visible.
            audio.spectralBalance = 0.62;
            audio.trebleShare = 0.4;
            const QImage mixLow = capture();
            audio.spectralBalance = 0.76;
            const QImage mixHigh = capture();
            double mixDifference = 0;
            for (int y = 0; y < mixLow.height(); ++y) {
                for (int x = 0; x < mixLow.width(); ++x) {
                    const auto a = mixLow.pixelColor(x, y), b = mixHigh.pixelColor(x, y);
                    if (a.alpha() > 30)
                        mixDifference += std::abs(a.red() - b.red()) + std::abs(a.green() - b.green()) + std::abs(a.blue() - b.blue());
                }
            }
            qInfo() << "Mean color change for a mixed passage" << mixDifference / (3 * visiblePixels);
            QVERIFY2(mixDifference / (3 * visiblePixels) > 4, "Mixed music has too little visible color range.");
            audio.spectralBalance = 0.98;
            audio.trebleShare = 0.9;
            // A stationary frame cannot acquire a color cycle from wall-clock time.
            QTest::qWait(120);
            QCOMPARE(capture(), highs);
            root->setProperty("dynamicColor", false);
            const QImage fixed = capture();
            audio.spectralBalance = 0.02;
            audio.trebleShare = 0;
            QCOMPARE(capture(), fixed);
            root->setProperty("dynamicColor", true);
            QCOMPARE(capture(), bass);
        }
        root->setProperty("reducedMotion", true);
        const QImage reducedBass = capture();
        audio.spectralBalance = 0.98;
        audio.trebleShare = 0.9;
        const QImage reducedHighs = capture();
        QVERIFY(reducedHighs != reducedBass); // Slow color changes remain accessible.
        audio.phaseOverride = 5;
        QCOMPARE(capture(), reducedHighs);
        audio.silent = true;
        const QImage silent = capture();
        for (int y = 0; y < silent.height(); ++y)
            for (int x = 0; x < silent.width(); ++x) QCOMPARE(silent.pixelColor(x, y).alpha(), 0);
        QVERIFY(!root->property("shaderFailed").toBool());
    }
    void musicalAccents_data() {
        QTest::addColumn<int>("band");
        QTest::addColumn<bool>("fallback");
        for (int band = 0; band < 3; ++band) {
            QTest::newRow(qPrintable(QStringLiteral("band-%1-shader").arg(band))) << band << false;
            QTest::newRow(qPrintable(QStringLiteral("band-%1-canvas").arg(band))) << band << true;
        }
    }
    void musicalAccents() {
        QFETCH(int, band);
        QFETCH(bool, fallback);
        TestAudio audio;
        audio.phaseOverride = 0.65;
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false}, {"forceFallback", fallback}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(200, 40);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const auto capture = [&] {
            QMetaObject::invokeMethod(view.rootObject(), "refresh");
            QTest::qWait(60);
            return view.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        };
        const QImage resting = capture();
        audio.accents[band] = 0.8;
        const QImage accented = capture();
        QVERIFY(!resting.isNull());
        QCOMPARE(accented.size(), resting.size());
        unsigned changed = 0;
        uint64_t baseLight = 0, accentLight = 0;
        for (int y = 0; y < resting.height(); ++y) {
            for (int x = 0; x < resting.width(); ++x) {
                for (int channel = 0; channel < 3; ++channel) {
                    const int a = resting.constScanLine(y)[x * 4 + channel];
                    const int b = accented.constScanLine(y)[x * 4 + channel];
                    changed += std::abs(a - b) > 2;
                    baseLight += a;
                    accentLight += b;
                }
            }
        }
        qInfo() << "Accent band" << band << "changed channels" << changed << "light ratio" << double(accentLight) / baseLight;
        QVERIFY2(changed > 80, "An isolated musical accent is not visible at panel size.");
        if (band == 2) QVERIFY(accentLight > baseLight * 1.005);
        // Every accent must remain gentle, with no large global light pulse.
        QVERIFY(accentLight < baseLight * 1.3);
        view.rootObject()->setProperty("reducedMotion", true);
        const QImage reduced = capture();
        audio.accents.fill(0);
        QCOMPARE(capture(), reduced);
        audio.silent = true;
        audio.accents.fill(1);
        const QImage silent = capture();
        for (int y = 0; y < silent.height(); ++y)
            for (int x = 0; x < silent.width(); ++x) QCOMPARE(silent.pixelColor(x, y).alpha(), 0);
    }
    void settingsRendering_data() {
        QTest::addColumn<int>("palette");
        QTest::addColumn<bool>("fallback");
        for (int palette = 0; palette < 6; ++palette) {
            QTest::newRow(qPrintable(QStringLiteral("palette-%1-shader").arg(palette))) << palette << false;
            QTest::newRow(qPrintable(QStringLiteral("palette-%1-canvas").arg(palette))) << palette << true;
        }
    }
    void settingsRendering() {
        QFETCH(int, palette);
        QFETCH(bool, fallback);
        TestAudio audio;
        audio.phaseOverride = 0.65;
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false},
            {"paletteIndex", palette}, {"forceFallback", fallback}, {"intensity", 0.4}, {"sensitivity", 0.5}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(200, 40);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *root = view.rootObject();
        const auto capture = [&] {
            QMetaObject::invokeMethod(root, "refresh");
            QTest::qWait(60);
            return view.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        };
        const auto alphaSum = [](const QImage &image) {
            uint64_t sum = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) sum += image.pixelColor(x, y).alpha();
            return sum;
        };
        const QImage low = capture();
        QVERIFY(!low.isNull());
        QCOMPARE(audio.lastSensitivity, 0.5);
        root->setProperty("intensity", 1.6);
        root->setProperty("sensitivity", 2.0);
        const QImage high = capture();
        QCOMPARE(audio.lastSensitivity, 2.0);
        QVERIFY(alphaSum(high) > alphaSum(low) * 1.5);
        uint64_t red = 0, green = 0, blue = 0;
        for (int y = 0; y < high.height(); ++y) {
            const auto *row = high.constScanLine(y);
            for (int x = 0; x < high.width(); ++x) {
                red += row[x * 4]; green += row[x * 4 + 1]; blue += row[x * 4 + 2];
            }
        }
        if (palette == 0) QVERIFY(green > red);
        else if (palette == 1) QVERIFY(red > blue);
        else if (palette == 2) QVERIFY(blue > red);
        else if (palette == 3) QVERIFY(green > red && green > blue);
        else if (palette == 4) QVERIFY(red > green && blue > green);
        else QVERIFY(red > green && red > blue);
        root->setProperty("reducedMotion", true);
        const QImage still = capture();
        audio.phaseOverride = 3.0;
        const QImage changedPhase = capture();
        if (changedPhase != still) {
            unsigned maximum = 0, changed = 0;
            QCOMPARE(changedPhase.size(), still.size());
            for (int y = 0; y < still.height(); ++y) {
                for (int x = 0; x < still.width() * 4; ++x) {
                    const auto difference = unsigned(std::abs(int(still.constScanLine(y)[x]) - int(changedPhase.constScanLine(y)[x])));
                    maximum = std::max(maximum, difference);
                    changed += difference != 0;
                }
            }
            qInfo() << "Reduced motion pixel difference" << maximum << "changed channels" << changed << "size" << still.size();
        }
        QCOMPARE(changedPhase, still);
        root->setProperty("reducedMotion", false);
        QVERIFY(capture() != still);
        // Returning from the fallback must restore the shader without a stale frame.
        root->setProperty("forceFallback", !fallback);
        QVERIFY(alphaSum(capture()) > 0);
        root->setProperty("forceFallback", fallback);
        QVERIFY(alphaSum(capture()) > 0);
        QVERIFY(!root->property("shaderFailed").toBool());
    }
    void phaseWrap_data() {
        QTest::addColumn<bool>("fallback");
        QTest::newRow("shader") << false;
        QTest::newRow("canvas") << true;
    }
    void themeContrast_data() { settingsRendering_data(); }
    void themeContrast() {
        QFETCH(int, palette);
        QFETCH(bool, fallback);
        TestAudio audio;
        audio.phaseOverride = 0.65;
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false},
            {"paletteIndex", palette}, {"forceFallback", fallback}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(200, 40);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *root = view.rootObject();
        const auto capture = [&] {
            QMetaObject::invokeMethod(root, "refresh");
            QTest::qWait(60);
            return view.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        };
        // RGB distance from white after source-over composition, not a WCAG ratio.
        const auto contrastOnWhite = [](const QImage &image) {
            uint64_t distance = 0;
            for (int y = 0; y < image.height(); ++y) {
                const auto *row = image.constScanLine(y);
                for (int x = 0; x < image.width(); ++x)
                    distance += 3 * row[x * 4 + 3] - row[x * 4] - row[x * 4 + 1] - row[x * 4 + 2];
            }
            return distance;
        };
        const QImage dark = capture();
        QVERIFY(!dark.isNull());
        root->setProperty("backdropColor", QColor("#ebedf0"));
        const QImage light = capture();
        QCOMPARE(light.size(), dark.size());
        QVERIFY(contrastOnWhite(light) > contrastOnWhite(dark) * 1.2);
        for (int y = 0; y < light.height(); ++y)
            for (int x = 0; x < light.width(); ++x)
                QCOMPARE(light.pixelColor(x, y).alpha(), dark.pixelColor(x, y).alpha());
        root->setProperty("backdropColor", QColor("#20242c"));
        QCOMPARE(capture(), dark);
        root->setProperty("backdropColor", QColor("#ebedf0"));
        audio.silent = true;
        const QImage silent = capture();
        for (int y = 0; y < silent.height(); ++y)
            for (int x = 0; x < silent.width(); ++x)
                QCOMPARE(silent.pixelColor(x, y).alpha(), 0);
    }
    void phaseWrap() {
        QFETCH(bool, fallback);
        TestAudio audio;
        audio.accents.fill(0.8);
        audio.phaseOverride = 200.0 * std::numbers::pi;
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false}, {"forceFallback", fallback}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(200, 40);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY(QMetaObject::invokeMethod(view.rootObject(), "refresh"));
        QTest::qWait(60);
        const QImage before = view.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        audio.phaseOverride = 0;
        QVERIFY(QMetaObject::invokeMethod(view.rootObject(), "refresh"));
        QTest::qWait(60);
        const QImage after = view.grabWindow().convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        QVERIFY(!before.isNull());
        QCOMPARE(before.size(), after.size());
        unsigned maximumDifference = 0;
        for (int y = 0; y < before.height(); ++y) {
            for (int x = 0; x < before.width() * 4; ++x) {
                maximumDifference = std::max(maximumDifference,
                    unsigned(std::abs(int(before.constScanLine(y)[x]) - int(after.constScanLine(y)[x]))));
            }
        }
        QVERIFY2(maximumDifference <= 2, qPrintable(QStringLiteral("Phase wrap changed a color channel by %1/255").arg(maximumDifference)));
    }
    void attackHasNoCentralCrease_data() {
        QTest::addColumn<double>("origin");
        QTest::newRow("legacy") << 0.46;
        QTest::newRow("bass") << 0.32;
        QTest::newRow("mid") << 0.50;
        QTest::newRow("treble") << 0.68;
    }
    void attackHasNoCentralCrease() {
        if (qEnvironmentVariable("QT_QUICK_BACKEND") == "software")
            QSKIP("The simple renderer intentionally has no travelling attack ripple.");
        TestAudio audio;
        QFETCH(double, origin);
        audio.rippleOrigin = origin;
        audio.phaseOverride = 0.65;
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        if (softwareRendererActive(view))
            QSKIP("The scene graph silently fell back to software; no travelling attack ripple.");
        view.resize(640, 190);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const auto capture = [&] {
            QMetaObject::invokeMethod(view.rootObject(), "refresh");
            QTest::qWait(60);
            return view.grabWindow();
        };
        const QImage resting = capture();
        audio.onset = 1.0;
        audio.rippleAge = 0.03;
        const QImage attack = capture();
        QVERIFY(!resting.isNull());
        QCOMPARE(attack.size(), resting.size());
        const auto centerAt = [](const QImage &image, int x) {
            double weightedY = 0, weight = 0;
            for (int y = 0; y < image.height(); ++y) {
                const int alpha = image.pixelColor(x, y).alpha();
                weightedY += y * alpha;
                weight += alpha;
            }
            return weightedY / std::max(1.0, weight);
        };
        const auto displacement = [&](int x) { return centerAt(attack, x) - centerAt(resting, x); };
        const int originPixel = int(std::round(attack.width() * origin - 0.5));
        double slopeJump = 0;
        for (int x = originPixel - 1; x <= originPixel + 1; ++x)
            slopeJump = std::max(slopeJump, std::abs(displacement(x - 1) + displacement(x + 1) - 2 * displacement(x)));
        qInfo() << "Attack origin slope change, pixels:" << slopeJump;
        QVERIFY(std::abs(displacement(originPixel)) > 0.5); // The ripple must remain visible.
        QVERIFY2(slopeJump < 0.15, "The attack ripple forms a sharp central crease.");
    }
    void rippleAtPanelSize() {
        if (qEnvironmentVariable("QT_QUICK_BACKEND") == "software")
            QSKIP("The simple renderer intentionally has no travelling attack ripple.");
        TestAudio audio;
        audio.phaseOverride = 0.65;
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}, {"viewEnabled", false}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        if (softwareRendererActive(view))
            QSKIP("The scene graph silently fell back to software; no travelling attack ripple.");
        view.resize(200, 40);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const auto capture = [&] {
            QMetaObject::invokeMethod(view.rootObject(), "refresh");
            QTest::qWait(60);
            return view.grabWindow();
        };
        const auto centerAt = [](const QImage &img, int x) {
            double sum = 0, mass = 0;
            for (int y = 0; y < img.height(); ++y) {
                const double weight = img.pixelColor(x, y).alphaF();
                sum += weight * y;
                mass += weight;
            }
            return sum / std::max(1e-9, mass);
        };
        const auto rest = capture();
        audio.onset = 0.9;
        audio.rippleAge = 0.03;
        for (const double origin : {0.32, 0.50, 0.68}) {
            audio.rippleOrigin = origin;
            const auto attack = capture();
            double sum = 0, mass = 0, maximum = 0;
            for (int x = 0; x < rest.width(); ++x) {
                const double displacement = std::abs(centerAt(attack, x) - centerAt(rest, x));
                sum += displacement * (x + 0.5) / rest.width();
                mass += displacement;
                maximum = std::max(maximum, displacement);
            }
            qInfo() << "Ripple origin" << origin << "rendered displacement center" << sum / mass << "peak px" << maximum;
            QVERIFY(maximum > 0.5 * view.devicePixelRatio());
            QVERIFY(std::abs(sum / mass - origin) < 0.07);
        }
        view.rootObject()->setProperty("reducedMotion", true);
        const auto reduced = capture();
        audio.rippleOrigin = 0.32;
        QCOMPARE(capture(), reduced);
    }
    void visibleHiddenSilentFallback() {
        TestAudio audio;
        QQuickView view;
        view.setColor(Qt::transparent);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setInitialProperties({{"audio", QVariant::fromValue(&audio)}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/RibbonView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(200, 40);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *root = view.rootObject();
        QTRY_VERIFY(audio.calls > 0);
        QSignalSpy changes(root, SIGNAL(frameChanged()));
        QTest::qWait(1100);
        qInfo() << "30 FPS limit: observed updates in 1.1 s:" << changes.count();
        QVERIFY(changes.count() >= 20 && changes.count() <= 34);
        QVERIFY(!root->property("shaderFailed").toBool());
        auto image = view.grabWindow();
        QVERIFY(!image.isNull());
        const auto litPixels = [](const QImage &img) {
            unsigned count = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) if (img.pixelColor(x, y).alpha() > 12) ++count;
            return count;
        };
        QVERIFY(litPixels(image) > 100);
        QVERIFY(litPixels(image) < unsigned(image.width() * image.height() / 2));
        root->setProperty("fps", 60);
        changes.clear();
        QTest::qWait(1100);
        qInfo() << "60 FPS limit: observed updates in 1.1 s:" << changes.count();
        QVERIFY(changes.count() >= 35 && changes.count() <= 67);
        root->setProperty("viewEnabled", false);
        QTest::qWait(80);
        const auto hiddenCalls = audio.calls;
        QTest::qWait(180);
        QCOMPARE(audio.calls, hiddenCalls);
        root->setProperty("viewEnabled", true);
        QTRY_VERIFY(audio.calls > hiddenCalls);
        view.hide();
        QTest::qWait(80);
        const auto windowHiddenCalls = audio.calls;
        QTest::qWait(180);
        QCOMPARE(audio.calls, windowHiddenCalls);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        root->setProperty("forceFallback", true);
        QTest::qWait(150);
        QVERIFY(root->property("fallback").toBool());
        QVERIFY(litPixels(view.grabWindow()) > 100);
        changes.clear();
        QTest::qWait(1100);
        qInfo() << "Canvas at 60 FPS: observed updates in 1.1 s:" << changes.count();
        QVERIFY(changes.count() >= 35 && changes.count() <= 67);
        root->setProperty("vertical", true);
        view.resize(40, 200);
        QTest::qWait(150);
        QVERIFY(litPixels(view.grabWindow()) > 100);
        audio.silent = true;
        QTest::qWait(150);
        auto frame = root->property("frame");
        if (frame.metaType() == QMetaType::fromType<QJSValue>()) frame = frame.value<QJSValue>().toVariant();
        QVERIFY(frame.toMap().contains("energy"));
        QCOMPARE(frame.toMap().value("energy").toDouble(), 0.0);
        changes.clear();
        QTest::qWait(300);
        QCOMPARE(changes.count(), 0);
        QVERIFY(litPixels(view.grabWindow()) == 0);
    }
};
QTEST_MAIN(ViewTests)
#include "test_view.moc"
