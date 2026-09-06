// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami

PlasmoidItem {
    id: root
    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    readonly property bool panel: Plasmoid.formFactor === PlasmaCore.Types.Horizontal || vertical
    readonly property var audio: Plasmoid.audio
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    preferredRepresentation: panel ? compactRepresentation : fullRepresentation
    toolTipMainText: "Luma Ribbon"
    toolTipSubText: panel ? qsTr("Light that follows your audio. Click to expand.")
        : qsTr("Light that follows your audio.")

    Binding {
        target: Plasmoid
        property: "previewSize"
        value: root.panel ? Qt.size(root.width, root.height) : Qt.size(200, 40)
        when: Plasmoid.previewSize !== undefined
    }

    component ConfiguredRibbon: RibbonView {
        audio: root.audio
        backdropColor: Kirigami.Theme.backgroundColor
        paletteIndex: Plasmoid.configuration.palette
        dynamicColor: Plasmoid.configuration.dynamicColor
        hue: Plasmoid.configuration.hue
        intensity: Plasmoid.configuration.intensity
        curvature: Plasmoid.configuration.curvature
        fullness: Plasmoid.configuration.fullness
        bloom: Plasmoid.configuration.bloom
        sensitivity: Plasmoid.configuration.sensitivity
        fps: Plasmoid.configuration.fps
        reducedMotion: Plasmoid.configuration.reducedMotion || Kirigami.Units.longDuration === 0
        forceFallback: Plasmoid.configuration.forceFallback
    }
    compactRepresentation: Item {
        Layout.minimumWidth: root.vertical ? 24 : 100
        Layout.preferredWidth: root.vertical ? 40 : 200
        Layout.maximumWidth: root.vertical ? Infinity : 300
        Layout.minimumHeight: root.vertical ? 100 : 24
        Layout.preferredHeight: root.vertical ? 200 : 40
        Layout.maximumHeight: root.vertical ? 300 : Infinity
        ConfiguredRibbon { anchors.fill: parent; vertical: root.vertical }
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: root.expanded = !root.expanded
        }
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Expand Luma Ribbon")
        Accessible.onPressAction: root.expanded = !root.expanded
    }
    fullRepresentation: Item {
        implicitWidth: 560
        implicitHeight: 260
        Layout.minimumWidth: 280
        Layout.minimumHeight: 140
        ConfiguredRibbon {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            viewEnabled: !root.panel || root.expanded
        }
    }
}
