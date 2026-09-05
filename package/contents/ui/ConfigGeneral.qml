// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.plasma.plasmoid
import "Palette.js" as Palette

KCM.SimpleKCM {
    id: root
    property alias cfg_palette: palette.currentIndex
    property alias cfg_dynamicColor: dynamicColor.checked
    property alias cfg_intensity: intensity.value
    property alias cfg_curvature: curvature.value
    property alias cfg_fullness: fullness.value
    property alias cfg_bloom: bloom.value
    property alias cfg_sensitivity: sensitivity.value
    property int cfg_fps: 30
    property alias cfg_reducedMotion: reduced.checked
    property alias cfg_forceFallback: fallback.checked
    // Plasma also supplies the generated defaults from KConfigPropertyMap.
    property int cfg_paletteDefault: 0
    property bool cfg_dynamicColorDefault: true
    property real cfg_intensityDefault: 1
    property real cfg_curvatureDefault: 1
    property real cfg_fullnessDefault: 1
    property real cfg_bloomDefault: 1
    property real cfg_sensitivityDefault: 1
    property int cfg_fpsDefault: 30
    property bool cfg_reducedMotionDefault
    property bool cfg_forceFallbackDefault
    readonly property var audio: Plasmoid.audio
    readonly property bool updatePending: !!audio && !(Plasmoid.appearanceRevision >= 2)
    readonly property size previewSize: Plasmoid.previewSize === undefined ? Qt.size(200, 40) : Plasmoid.previewSize

    function resetAppearance() {
        cfg_palette = cfg_paletteDefault;
        cfg_dynamicColor = cfg_dynamicColorDefault;
        cfg_intensity = cfg_intensityDefault;
        cfg_curvature = cfg_curvatureDefault;
        cfg_fullness = cfg_fullnessDefault;
        cfg_bloom = cfg_bloomDefault;
    }

    // Keep the result visible while scrolling the controls on a small screen.
    header: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing
        Controls.Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.smallSpacing
            text: qsTr("Live preview")
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: preview.height + Kirigami.Units.largeSpacing * 2
            color: Kirigami.Theme.backgroundColor
            RibbonView {
                id: preview
                objectName: "appearancePreview"
                anchors.centerIn: parent
                width: root.previewSize.width * Math.min(1, parent.width / Math.max(1, root.previewSize.width))
                height: root.previewSize.height * Math.min(1, parent.width / Math.max(1, root.previewSize.width))
                vertical: root.previewSize.height > root.previewSize.width
                audio: root.audio
                viewEnabled: !!root.audio && root.visible
                reportStatus: false
                backdropColor: Kirigami.Theme.backgroundColor
                paletteIndex: root.cfg_palette
                dynamicColor: root.cfg_dynamicColor
                intensity: root.cfg_intensity
                curvature: root.cfg_curvature
                fullness: root.cfg_fullness
                bloom: root.cfg_bloom
                sensitivity: root.cfg_sensitivity
                fps: root.cfg_fps
                reducedMotion: root.cfg_reducedMotion || Kirigami.Units.longDuration === 0
                forceFallback: root.cfg_forceFallback
            }
        }
        Controls.Label {
            Layout.fillWidth: true
            Layout.bottomMargin: Kirigami.Units.smallSpacing
            horizontalAlignment: Text.AlignHCenter
            text: root.updatePending ? qsTr("Log out and back in to enable the new appearance controls.")
                : !root.audio || root.audio.hasError ? qsTr("Preview unavailable. Check audio output below.")
                : preview.frame.energy === 0 ? qsTr("Play audio to preview. Apply saves your changes.")
                : qsTr("Apply saves your changes.")
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            opacity: 0.7
        }
    }

    Kirigami.FormLayout {
        Controls.ComboBox {
            id: palette
            Kirigami.FormData.label: qsTr("Palette:")
            model: Palette.names()
        }
        Controls.CheckBox {
            id: dynamicColor
            text: qsTr("Audio-reactive colors")
            Controls.ToolTip.text: qsTr("Gently shifts colors within the selected palette as bass, mids and highs change. Disable for a fixed gradient.")
            Controls.ToolTip.visible: hovered
        }
        RowLayout {
            Kirigami.FormData.label: qsTr("Light intensity:")
            Controls.Slider {
                id: intensity
                Layout.preferredWidth: 220
                Layout.fillWidth: true
                from: 0.4; to: 1.6; stepSize: 0.05
                Accessible.name: qsTr("Light intensity")
            }
            Controls.Label {
                text: qsTr("%1%", "Percentage").arg(Math.round(intensity.value * 100))
                Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                horizontalAlignment: Text.AlignRight
            }
        }
        AppearanceControl {
            id: curvature
            objectName: "curvatureControl"
            enabled: !root.updatePending
            Kirigami.FormData.label: qsTr("Curvature:")
            accessibleName: qsTr("Curvature")
            from: 0.5; to: 1.25
            defaultValue: root.cfg_curvatureDefault
            lowText: qsTr("Subtle")
            highText: qsTr("Pronounced")
        }
        AppearanceControl {
            id: fullness
            objectName: "fullnessControl"
            enabled: !root.updatePending
            Kirigami.FormData.label: qsTr("Ribbon fullness:")
            accessibleName: qsTr("Ribbon fullness")
            from: 0.6; to: 1.3
            defaultValue: root.cfg_fullnessDefault
            lowText: qsTr("Fine")
            highText: qsTr("Full")
        }
        AppearanceControl {
            id: bloom
            objectName: "bloomControl"
            enabled: !root.updatePending
            Kirigami.FormData.label: qsTr("Bloom:")
            accessibleName: qsTr("Bloom")
            from: 0; to: 1.5
            defaultValue: root.cfg_bloomDefault
            lowText: qsTr("Off")
            highText: qsTr("Strong")
        }
        Controls.Button {
            objectName: "resetAppearance"
            text: qsTr("Reset appearance")
            icon.name: "edit-undo"
            enabled: !root.updatePending && (root.cfg_palette !== root.cfg_paletteDefault
                || root.cfg_dynamicColor !== root.cfg_dynamicColorDefault
                || Math.abs(root.cfg_intensity - root.cfg_intensityDefault) > 0.001
                || Math.abs(root.cfg_curvature - root.cfg_curvatureDefault) > 0.001
                || Math.abs(root.cfg_fullness - root.cfg_fullnessDefault) > 0.001
                || Math.abs(root.cfg_bloom - root.cfg_bloomDefault) > 0.001)
            onClicked: root.resetAppearance()
            Controls.ToolTip.text: qsTr("Resets palette, audio-reactive colors, light intensity, curvature, fullness and bloom. Apply to save.")
            Controls.ToolTip.visible: hovered
        }
        Kirigami.Separator { Kirigami.FormData.isSection: true }
        RowLayout {
            Kirigami.FormData.label: qsTr("Audio sensitivity:")
            Controls.Slider {
                id: sensitivity
                Layout.preferredWidth: 220
                Layout.fillWidth: true
                from: 0.5; to: 2.0; stepSize: 0.05
                Accessible.name: qsTr("Audio sensitivity")
            }
            Controls.Label {
                text: qsTr("%1%", "Percentage").arg(Math.round(sensitivity.value * 100))
                Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                horizontalAlignment: Text.AlignRight
            }
        }
        Controls.ComboBox {
            Kirigami.FormData.label: qsTr("Frame limit:")
            model: [qsTr("30 FPS"), qsTr("60 FPS")]
            currentIndex: root.cfg_fps === 60 ? 1 : 0
            onActivated: root.cfg_fps = currentIndex === 1 ? 60 : 30
        }
        Controls.CheckBox {
            id: reduced
            text: qsTr("Reduced motion")
            Controls.ToolTip.text: qsTr("Keeps color and intensity responsive without travelling waves or ripples.")
            Controls.ToolTip.visible: hovered
        }
        Controls.CheckBox {
            id: fallback
            text: qsTr("Simple rendering")
            Controls.ToolTip.text: qsTr("Uses a glowing curve without shaders. Enabled automatically with the software renderer.")
            Controls.ToolTip.visible: hovered
        }
        Kirigami.Separator { Kirigami.FormData.isSection: true }
        Controls.Label {
            Kirigami.FormData.label: qsTr("Audio output:")
            text: root.audio ? (root.audio.deviceName || qsTr("No output available")) : qsTr("Native module not loaded")
            wrapMode: Text.Wrap
            Layout.maximumWidth: 360
        }
        Controls.Label {
            text: root.audio ? root.audio.statusMessage : qsTr("Install the Luma Ribbon C++ library as well as the widget package.")
            color: root.audio && !root.audio.hasError ? Kirigami.Theme.textColor : Kirigami.Theme.negativeTextColor
            wrapMode: Text.Wrap
            Layout.maximumWidth: 360
        }
        Controls.Label {
            text: root.audio ? root.audio.formatDescription : ""
            opacity: 0.7
            visible: text.length > 0
        }
        Controls.Label {
            text: root.audio ? root.audio.renderingStatus : ""
            wrapMode: Text.Wrap
            Layout.maximumWidth: 360
            visible: text.length > 0
            opacity: 0.7
        }
        Controls.Label {
            text: qsTr("Listens only to the default output monitor. The microphone is never used. The ribbon fades away during silence.")
            wrapMode: Text.Wrap
            Layout.maximumWidth: 360
            opacity: 0.7
        }
    }
}
