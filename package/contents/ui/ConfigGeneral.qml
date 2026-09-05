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
    property alias cfg_sensitivity: sensitivity.value
    property int cfg_fps: 30
    property alias cfg_reducedMotion: reduced.checked
    property alias cfg_forceFallback: fallback.checked
    // Plasma also supplies the generated defaults from KConfigPropertyMap.
    property int cfg_paletteDefault
    property bool cfg_dynamicColorDefault
    property real cfg_intensityDefault
    property real cfg_sensitivityDefault
    property int cfg_fpsDefault
    property bool cfg_reducedMotionDefault
    property bool cfg_forceFallbackDefault
    readonly property var audio: Plasmoid.audio

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
