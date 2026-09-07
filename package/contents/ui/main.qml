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
    readonly property int panelLength: Plasmoid.configuration.panelLength === undefined ? 120
        : Math.max(80, Math.min(160, Plasmoid.configuration.panelLength))
    readonly property var audio: Plasmoid.audio
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    preferredRepresentation: panel ? compactRepresentation : fullRepresentation
    toolTipMainText: "Luma Ribbon"
    toolTipSubText: panel ? qsTr("Light that follows your audio. Click to expand.")
        : qsTr("Light that follows your audio.")

    Binding {
        target: Plasmoid
        property: "previewSize"
        value: root.panel ? Qt.size(root.width, root.height) : Qt.size(root.panelLength, 40)
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
        // Reserve a fixed length even during silence; only panel thickness can grow.
        Layout.minimumWidth: root.vertical ? 24 : root.panelLength
        Layout.preferredWidth: root.vertical ? 40 : root.panelLength
        Layout.maximumWidth: root.vertical ? Infinity : root.panelLength
        Layout.minimumHeight: root.vertical ? root.panelLength : 24
        Layout.preferredHeight: root.vertical ? root.panelLength : 40
        Layout.maximumHeight: root.vertical ? root.panelLength : Infinity
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
