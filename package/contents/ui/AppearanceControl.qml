// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    property alias value: slider.value
    property alias from: slider.from
    property alias to: slider.to
    required property real defaultValue
    required property string accessibleName
    required property string lowText
    required property string highText
    spacing: 0

    RowLayout {
        Layout.fillWidth: true
        Controls.Slider {
            id: slider
            objectName: root.objectName + "Slider"
            Layout.preferredWidth: 220
            Layout.fillWidth: true
            value: root.defaultValue
            stepSize: 0.05
            snapMode: Controls.Slider.SnapAlways
            live: true
            Accessible.name: root.accessibleName
            Accessible.description: qsTr("Default: %1%").arg(Math.round(root.defaultValue * 100))
        }
        Controls.Label {
            text: qsTr("%1%", "Percentage").arg(Math.round(slider.value * 100))
            Layout.minimumWidth: Kirigami.Units.gridUnit * 3
            horizontalAlignment: Text.AlignRight
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Controls.Label { text: root.lowText; font: Kirigami.Theme.smallFont; opacity: 0.7 }
        Controls.Label {
            text: qsTr("Default %1%").arg(Math.round(root.defaultValue * 100))
            font: Kirigami.Theme.smallFont
            opacity: 0.7
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
        Controls.Label { text: root.highText; font: Kirigami.Theme.smallFont; opacity: 0.7 }
    }
}
