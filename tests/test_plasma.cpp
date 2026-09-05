// SPDX-License-Identifier: GPL-3.0-or-later
#include <Plasma/Applet>
#include <Plasma/Containment>
#include <Plasma/Corona>
#include <PlasmaQuick/AppletQuickItem>
#include <KPluginMetaData>
#include <KConfigPropertyMap>
#include <KSharedConfig>
#include <KConfigGroup>
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
#include <QTemporaryDir>
#include <cmath>

class TestCorona : public Plasma::Corona {
public:
    QRect screenGeometry(int) const override { return QRect(0, 0, 1024, 768); }
};
class PlasmaTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void appletPopupConfigAndRemoval() {
        QTemporaryDir settings;
        QVERIFY(settings.isValid());
        TestCorona corona;
        auto shell = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/Shell"));
        shell.setPath(QStringLiteral("org.kde.plasma.desktop"));
        QVERIFY(shell.isValid());
        corona.setKPackage(shell);
        corona.loadLayout(settings.filePath(QStringLiteral("plasma-appletsrc")));
        QCOMPARE(corona.config()->name(), settings.filePath(QStringLiteral("plasma-appletsrc")));
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
        for (const auto *key : {"curvature", "fullness"}) {
            QCOMPARE(first->configuration()->value(QString::fromLatin1(key)).toDouble(), 1.0);
            QCOMPARE(panelRibbon->property(key).toDouble(), 1.0);
            QCOMPARE(popupRibbon->property(key).toDouble(), 1.0);
        }
        QCOMPARE(first->property("previewSize").toSizeF(), QSizeF(200, 40));
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
            {"cfg_curvature", 0.8}, {"cfg_fullness", 1.15},
            {"cfg_curvatureDefault", 1.0}, {"cfg_fullnessDefault", 1.0},
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
        QVERIFY(!form->property("updatePending").toBool());
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
        settingsWindow.requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(&settingsWindow));
        auto *preview = page->findChild<QQuickItem *>(QStringLiteral("appearancePreview"));
        QVERIFY(preview);
        QCOMPARE(preview->property("audio").value<QObject *>(), audio.data());
        QCOMPARE(preview->size(), QSizeF(200, 40));
        QTRY_VERIFY(preview->property("renderActive").toBool());
        QCOMPARE(preview->property("curvature").toDouble(), 0.8);
        QCOMPARE(preview->property("fullness").toDouble(), 1.15);
        QCOMPARE(preview->property("fps").toInt(), 60);
        QVERIFY(!preview->property("reportStatus").toBool());
        const auto renderingStatus = audio->property("renderingStatus");
        form->setProperty("cfg_forceFallback", true);
        QTRY_VERIFY(preview->property("fallback").toBool());
        QCOMPARE(audio->property("renderingStatus"), renderingStatus);
        // Draft edits reach the preview during a drag, before release or Apply.
        auto *curveSlider = page->findChild<QQuickItem *>(QStringLiteral("curvatureControlSlider"));
        auto *fullSlider = page->findChild<QQuickItem *>(QStringLiteral("fullnessControlSlider"));
        QVERIFY(curveSlider && fullSlider);
        for (auto *slider : {curveSlider, fullSlider}) {
            const auto position = slider->mapToScene(QPointF(slider->width() * 0.8, slider->height() / 2)).toPoint();
            QVERIFY(position.y() > 0 && position.y() < settingsWindow.height());
            auto *handle = slider->property("handle").value<QQuickItem *>();
            QVERIFY(handle);
            const auto start = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2)).toPoint();
            QTest::mousePress(&settingsWindow, Qt::LeftButton, Qt::NoModifier, start);
            QTest::mouseMove(&settingsWindow, position, 20);
            QVERIFY(slider->property("pressed").toBool());
            const char *key = slider == curveSlider ? "curvature" : "fullness";
            QTRY_COMPARE(preview->property(key), slider->property("value"));
            QVERIFY(preview->property(key).toDouble() > 1.0);
            QCOMPARE(panelRibbon->property(key).toDouble(), 1.0);
            QCOMPARE(popupRibbon->property(key).toDouble(), 1.0);
            QTest::mouseRelease(&settingsWindow, Qt::LeftButton, Qt::NoModifier, position);
            slider->forceActiveFocus(Qt::TabFocusReason);
            QTRY_COMPARE(settingsWindow.activeFocusItem(), slider);
            const double beforeKey = slider->property("value").toDouble();
            QTest::keyClick(&settingsWindow, Qt::Key_Left);
            QTRY_VERIFY(slider->property("value").toDouble() < beforeKey);
            QCOMPARE(preview->property(key), slider->property("value"));
        }
        // Reset only appearance. Keep audio, frame rate and accessibility choices.
        form->setProperty("cfg_reducedMotion", true);
        auto *reset = page->findChild<QQuickItem *>(QStringLiteral("resetAppearance"));
        QVERIFY(reset && reset->isEnabled());
        const auto resetPosition = reset->mapToScene(QPointF(reset->width() / 2, reset->height() / 2)).toPoint();
        QVERIFY(resetPosition.y() < settingsWindow.height());
        QTest::mouseClick(&settingsWindow, Qt::LeftButton, Qt::NoModifier, resetPosition);
        for (const auto *key : {"palette", "dynamicColor", "intensity", "curvature", "fullness"}) {
            const QByteArray setting = QByteArray("cfg_") + key;
            QCOMPARE(form->property(setting), form->property(setting + "Default"));
        }
        QVERIFY(!reset->isEnabled());
        QCOMPARE(form->property("cfg_sensitivity").toDouble(), 1.5);
        QCOMPARE(form->property("cfg_fps").toInt(), 60);
        QVERIFY(form->property("cfg_reducedMotion").toBool());
        QVERIFY(form->property("cfg_forceFallback").toBool());
        // Mimic Apply through the same KConfigPropertyMap used by Plasma.
        first->configuration()->insert(QStringLiteral("curvature"), 1.2);
        first->configuration()->insert(QStringLiteral("fullness"), 0.75);
        first->configuration()->writeConfig();
        corona.requireConfigSync();
        KConfig saved(settings.filePath(QStringLiteral("plasma-appletsrc")), KConfig::SimpleConfig);
        const auto general = saved.group(QStringLiteral("Containments")).group(QString::number(containment->id()))
            .group(QStringLiteral("Applets")).group(QString::number(first->id())).group(QStringLiteral("Configuration"))
            .group(QStringLiteral("General"));
        QCOMPARE(general.readEntry("curvature", 0.0), 1.2);
        QCOMPARE(general.readEntry("fullness", 0.0), 0.75);
        QTRY_COMPARE(panelRibbon->property("curvature").toDouble(), 1.2);
        QTRY_COMPARE(popupRibbon->property("fullness").toDouble(), 0.75);
        settingsWindow.hide();
        QTRY_VERIFY(!preview->property("renderActive").toBool());
        settingsWindow.show();
        QVERIFY(QTest::qWaitForWindowExposed(&settingsWindow));
        QTRY_VERIFY(preview->property("renderActive").toBool());
        page->setVisible(false);
        QTRY_VERIFY(!preview->property("renderActive").toBool());
        page->setVisible(true);
        QTRY_VERIFY(preview->property("renderActive").toBool());
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
            // A held fixture documents the controls without playing test sound.
            preview->setProperty("viewEnabled", false);
            preview->setProperty("paletteIndex", 1);
            preview->setProperty("reducedMotion", false);
            preview->setProperty("forceFallback", false);
            preview->setProperty("frame", QVariantMap{{"energy", 0.8}, {"bass", 0.6}, {"mid", 0.6}, {"treble", 0.4},
                {"phase", 0.65}, {"onset", 0.0}, {"rippleAge", 10.0}, {"arch", 0.2}, {"counterBend", 1.0}, {"bias", 0.0}, {"opening", 0.7}});
            form->setProperty("cfg_palette", 1);
            form->setProperty("cfg_reducedMotion", false);
            form->setProperty("cfg_forceFallback", false);
            QTest::mouseMove(&settingsWindow, QPoint(600, 520));
            QTest::qWait(1100);
            QVERIFY(settingsWindow.grabWindow().save(settingsCapture));
        }
        containment->setFormFactor(Plasma::Types::Vertical);
        containment->setLocation(Plasma::Types::LeftEdge);
        containment->flushPendingConstraintsEvents();
        first->flushPendingConstraintsEvents();
        window.resize(40, 200);
        item->setSize(QSizeF(40, 200));
        QTRY_VERIFY(item->property("vertical").toBool());
        QTRY_COMPARE(first->property("previewSize").toSizeF(), QSizeF(40, 200));
        QTRY_COMPARE(preview->size(), QSizeF(40, 200));
        QTRY_VERIFY(preview->property("vertical").toBool());
        form.reset();
        settingsWindow.hide();
        QTest::qWait(80);
        auto *second = containment->createApplet(QStringLiteral("org.kde.plasma.lumaribbon"));
        QVERIFY(second && !second->failedToLaunch());
        QCOMPARE(second->configuration()->value(QStringLiteral("curvature")).toDouble(), 1.0);
        QCOMPARE(second->configuration()->value(QStringLiteral("fullness")).toDouble(), 1.0);
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
