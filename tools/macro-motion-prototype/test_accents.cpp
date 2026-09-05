// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QTest>
#include <QImage>
#include <QTransform>
#include <QVariantMap>
#include <algorithm>
#include <cmath>

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
    view.rootObject()->setProperty("frame", frame);
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

class PrototypeAccentTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
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
        view.setInitialProperties({{"candidate", true}, {"dynamicColor", false},
            {"forceFallback", fallback}, {"vertical", vertical}, {"frame", frame}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_PROTOTYPE_DIR "/PrototypeRibbon.qml")));
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
        view.setInitialProperties({{"candidate", true}, {"dynamicColor", false},
            {"forceFallback", fallback}, {"frame", frame}});
        view.setSource(QUrl::fromLocalFile(QStringLiteral(LUMA_PROTOTYPE_DIR "/PrototypeRibbon.qml")));
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
    PrototypeAccentTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "test_accents.moc"
