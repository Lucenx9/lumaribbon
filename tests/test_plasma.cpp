// SPDX-License-Identifier: GPL-3.0-or-later
#include <Plasma/Applet>
#include <Plasma/Containment>
#include <Plasma/Corona>
#include <PlasmaQuick/AppletQuickItem>
#include <KPluginMetaData>
#include <KConfigPropertyMap>
#include <KPackage/PackageLoader>
#include <KPackage/Package>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QPointer>
#include <QTest>
#include <QSignalSpy>
#include <QStringList>
#include <cmath>

class TestCorona : public Plasma::Corona {
public:
    QRect screenGeometry(int) const override { return QRect(0, 0, 1024, 768); }
};
class PlasmaTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void appletPopupConfigAndRemoval() {
        TestCorona corona;
        auto shell = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/Shell"));
        shell.setPath(QStringLiteral("org.kde.plasma.desktop"));
        QVERIFY(shell.isValid());
        corona.setKPackage(shell);
        auto *containment = new Plasma::Containment(&corona, KPluginMetaData(), {});
        containment->init();
        containment->updateConstraints(Plasma::Applet::StartupCompletedConstraint);
        containment->flushPendingConstraintsEvents();
        containment->setFormFactor(Plasma::Types::Horizontal);
        containment->setLocation(Plasma::Types::BottomEdge);
        auto *first = containment->createApplet(QStringLiteral("org.kde.plasma.lumaribbon"));
        QVERIFY(first);
        first->updateConstraints(Plasma::Applet::StartupCompletedConstraint);
        first->flushPendingConstraintsEvents();
        QVERIFY2(!first->failedToLaunch(), qPrintable(first->launchErrorMessage()));
        QPointer<QObject> audio = first->property("audio").value<QObject *>();
        QVERIFY(audio);
        QPointer<PlasmaQuick::AppletQuickItem> item = PlasmaQuick::AppletQuickItem::itemForApplet(first);
        QVERIFY(item);
        QVERIFY2(!first->failedToLaunch(), qPrintable(first->launchErrorMessage()));
        QQuickWindow window;
        window.resize(200, 40);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(200, 40));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_VERIFY(item->compactRepresentationItem());
        QVERIFY(item->compactRepresentationItem()->isVisible());
        QCOMPARE(item->property("audio").value<QObject *>(), audio.data());
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(100, 20));
        QTRY_VERIFY(item->isExpanded());
        QTRY_VERIFY(item->fullRepresentationItem());
        QTRY_VERIFY(item->fullRepresentationItem()->window() && item->fullRepresentationItem()->window()->isVisible());
        QObject *popupRibbon = nullptr;
        for (auto *child : item->fullRepresentationItem()->findChildren<QObject *>()) {
            if (child->metaObject()->indexOfProperty("renderActive") >= 0) { popupRibbon = child; break; }
        }
        QVERIFY(popupRibbon);
        QCOMPARE(popupRibbon->property("audio").value<QObject *>(), audio.data());
        QVERIFY(popupRibbon->property("dynamicColor").toBool());
        QObject *panelRibbon = nullptr;
        for (auto *child : item->compactRepresentationItem()->findChildren<QObject *>()) {
            if (child->metaObject()->indexOfProperty("renderActive") >= 0) { panelRibbon = child; break; }
        }
        QVERIFY(panelRibbon);
        QCOMPARE(first->configuration()->value(QStringLiteral("dynamicColorDefault")).toBool(), true);
        first->configuration()->insert(QStringLiteral("dynamicColor"), false);
        QTRY_VERIFY(!panelRibbon->property("dynamicColor").toBool());
        QTRY_VERIFY(!popupRibbon->property("dynamicColor").toBool());
        first->configuration()->insert(QStringLiteral("dynamicColor"), true);
        QTRY_VERIFY(panelRibbon->property("dynamicColor").toBool());
        QTRY_VERIFY(popupRibbon->property("dynamicColor").toBool());
        QTRY_VERIFY(popupRibbon->property("renderActive").toBool());
        QTest::qWait(150);
        item->setExpanded(false);
        QVERIFY(!item->isExpanded());
        QTRY_VERIFY(!popupRibbon->property("renderActive").toBool());

        QQmlComponent config(qmlEngine(item), QUrl::fromLocalFile(QStringLiteral(LUMA_SOURCE_DIR "/package/contents/ui/ConfigGeneral.qml")));
        // Plasma 6.7 passes the page title and every KConfigPropertyMap key,
        // including the generated *Default entries, as initial properties.
        const QVariantMap initialSettings{{"title", "Appearance"},
            {"cfg_palette", 0}, {"cfg_intensity", 1.2}, {"cfg_sensitivity", 1.5},
            {"cfg_dynamicColor", true}, {"cfg_dynamicColorDefault", true},
            {"cfg_fps", 60}, {"cfg_reducedMotion", false}, {"cfg_forceFallback", false},
            {"cfg_paletteDefault", 0}, {"cfg_intensityDefault", 1.0}, {"cfg_sensitivityDefault", 1.0},
            {"cfg_fpsDefault", 30}, {"cfg_reducedMotionDefault", false}, {"cfg_forceFallbackDefault", false}};
        QSignalSpy configWarnings(qmlEngine(item), &QQmlEngine::warnings);
        QScopedPointer<QObject> form(config.createWithInitialProperties(initialSettings, QQmlEngine::contextForObject(item)));
        QVERIFY2(form, qPrintable(config.errorString()));
        for (const auto &arguments : configWarnings) {
            for (const auto &warning : qvariant_cast<QList<QQmlError>>(arguments.at(0)))
                QVERIFY2(!warning.description().contains(QStringLiteral("Setting initial properties failed")), qPrintable(warning.toString()));
        }
        for (auto it = initialSettings.cbegin(); it != initialSettings.cend(); ++it)
            QCOMPARE(form->property(qPrintable(it.key())), it.value());
        QCOMPARE(form->property("audio").value<QObject *>(), audio.data());
        QObject *paletteControl = nullptr;
        for (auto *child : form->findChildren<QObject *>()) {
            if (child->metaObject()->indexOfProperty("currentText") >= 0 && child->property("count").toInt() == 6) {
                paletteControl = child;
                break;
            }
        }
        QVERIFY(paletteControl);
        const QStringList paletteNames{QStringLiteral("Aurora"), QStringLiteral("Ember"), QStringLiteral("Ice"),
            QStringLiteral("Grove"), QStringLiteral("Iris"), QStringLiteral("Coral")};
        for (int palette = 0; palette < paletteNames.size(); ++palette) {
            QVERIFY(form->setProperty("cfg_palette", palette));
            QCOMPARE(form->property("cfg_palette").toInt(), palette);
            QCOMPARE(paletteControl->property("currentText").toString(), paletteNames[palette]);
            first->configuration()->insert(QStringLiteral("palette"), palette);
            QTRY_COMPARE(panelRibbon->property("paletteIndex").toInt(), palette);
            QTRY_COMPARE(popupRibbon->property("paletteIndex").toInt(), palette);
        }
        QVERIFY(form->setProperty("cfg_fps", 60));
        QCOMPARE(form->property("cfg_fps").toInt(), 60);
        QQuickWindow settingsWindow;
        settingsWindow.resize(640, 560);
        auto *page = qobject_cast<QQuickItem *>(form.data());
        QVERIFY(page);
        page->setParentItem(settingsWindow.contentItem());
        page->setSize(QSizeF(640, 560));
        settingsWindow.show();
        QVERIFY(QTest::qWaitForWindowExposed(&settingsWindow));
        QQuickItem *colorToggle = nullptr;
        for (auto *child : page->findChildren<QQuickItem *>()) {
            if (child->property("text").toString() == QStringLiteral("Audio-reactive colors")
                && child->metaObject()->indexOfProperty("checked") >= 0) { colorToggle = child; break; }
        }
        QVERIFY(colorToggle && colorToggle->isVisible());
        const auto togglePosition = colorToggle->mapToScene(QPointF(colorToggle->width() / 2, colorToggle->height() / 2)).toPoint();
        QVERIFY(settingsWindow.geometry().size().width() > togglePosition.x() && togglePosition.x() > 0);
        QVERIFY(settingsWindow.geometry().size().height() > togglePosition.y() && togglePosition.y() > 0);
        QTest::mouseClick(&settingsWindow, Qt::LeftButton, Qt::NoModifier, togglePosition);
        QCOMPARE(form->property("cfg_dynamicColor").toBool(), false);
        QTest::mouseClick(&settingsWindow, Qt::LeftButton, Qt::NoModifier, togglePosition);
        QCOMPARE(form->property("cfg_dynamicColor").toBool(), true);
        const auto settingsCapture = qEnvironmentVariable("LUMA_SETTINGS_CAPTURE");
        if (!settingsCapture.isEmpty()) {
            QTest::mouseMove(&settingsWindow, QPoint(600, 520));
            QTest::qWait(1100);
            QVERIFY(settingsWindow.grabWindow().save(settingsCapture));
        }
        form.reset();
        settingsWindow.hide();

        containment->setFormFactor(Plasma::Types::Vertical);
        containment->setLocation(Plasma::Types::LeftEdge);
        containment->flushPendingConstraintsEvents();
        first->flushPendingConstraintsEvents();
        window.resize(40, 200);
        item->setSize(QSizeF(40, 200));
        QTRY_VERIFY(item->property("vertical").toBool());
        QTest::qWait(80);
        auto *second = containment->createApplet(QStringLiteral("org.kde.plasma.lumaribbon"));
        QVERIFY(second && !second->failedToLaunch());
        QPointer<QObject> secondAudio = second->property("audio").value<QObject *>();
        QVERIFY(secondAudio && secondAudio != audio);
        delete first;
        QTRY_VERIFY(audio.isNull());
        QTRY_VERIFY(item.isNull());
        QVERIFY(secondAudio);
        QVariantMap snapshot;
        QVERIFY(QMetaObject::invokeMethod(secondAudio, "sample", Q_RETURN_ARG(QVariantMap, snapshot), Q_ARG(double, 1.0)));
        for (const auto *key : {"energy", "bassAccent", "midAccent", "trebleAccent", "rippleAge", "rippleOrigin", "spectralBalance", "trebleShare"}) {
            QVERIFY(snapshot.contains(key));
            QVERIFY(std::isfinite(snapshot.value(key).toDouble()));
        }
        delete second;
        QTRY_VERIFY(secondAudio.isNull());
        delete containment;
    }
};
QTEST_MAIN(PlasmaTests)
#include "test_plasma.moc"
