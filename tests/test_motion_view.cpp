// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QTest>
#include <QSignalSpy>
#include <QImage>
#include <QTransform>
#include <QVariantMap>
#include <algorithm>
#include <cmath>

class FrameAudio : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current MEMBER current)
public:
    QVariantMap current;
    Q_INVOKABLE QVariantMap sample(double) const { return current; }
};

namespace {
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
}

class MotionViewTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
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
