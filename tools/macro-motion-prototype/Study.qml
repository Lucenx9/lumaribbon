// SPDX-License-Identifier: GPL-3.0-or-later
// Disposable Qt comparison. Every ribbon receives the exact same published frame.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import "../../package/contents/ui/Palette.js" as Palette

Rectangle {
    id: root
    required property var frame
    required property bool syntheticInput
    required property bool isolateShape
    required property bool reduceMotion
    required property bool simpleRendering
    required property int paletteChoice
    property string stage: "Connecting to the default output"
    property real audioTime: 0
    readonly property var visualFrame: isolateShape
        ? Object.assign({}, frame, { onset: 0, bassAccent: 0, midAccent: 0, trebleAccent: 0 }) : frame
    width: 1000; height: 800
    color: "#151920"

    component Label: Text { color: "#a9b1c0"; font.pixelSize: 12 }
    component Ribbon: PrototypeRibbon {
        frame: root.visualFrame
        dynamicColor: !root.isolateShape
        reducedMotion: root.reduceMotion
        forceFallback: root.simpleRendering
        paletteIndex: root.paletteChoice
    }
    Column {
        x: 32; y: 24; spacing: 10
        Text { text: "Luma Ribbon / Shape study"; color: "#eef1f6"; font.pixelSize: 24 }
        Label { text: root.syntheticInput ? "Synthetic audio · FFTW analysis · no sound is played" : "Live output monitor · one analysis shared by both versions" }
        Label { text: root.stage + " · " + root.audioTime.toFixed(1) + " s"; color: "#d1d8e3" }
    }
    Row {
        x: 32; y: 120; spacing: 24
        Repeater {
            model: 2
            Column {
                id: version
                required property int index
                spacing: 12
                Text {
                    text: version.index === 0 ? "Earlier travelling-wave baseline" : "Design study · audio-shaped curvature"
                    color: "#eef1f6"; font.pixelSize: 16
                }
                Label { text: "Panel · 200 × 40 and 160 × 32 logical pixels" }
                Rectangle {
                    width: 456; height: 72; color: "#252a33"; radius: 8
                    Ribbon { x: 20; anchors.verticalCenter: parent.verticalCenter; width: 200; height: 40; candidate: version.index === 1 }
                    Rectangle { x: 239; y: 16; width: 1; height: 40; color: "#3a404b" }
                    Ribbon { x: 266; anchors.verticalCenter: parent.verticalCenter; width: 160; height: 32; candidate: version.index === 1 }
                }
                Label { text: "Expanded · same shape and audio state" }
                Rectangle {
                    width: 456; height: 198; color: "#1d222b"; radius: 8
                    Ribbon { anchors.centerIn: parent; width: 432; height: 180; candidate: version.index === 1 }
                }
                Row {
                    spacing: 14
                    Rectangle {
                        width: 72; height: 220; color: "#252a33"; radius: 8
                        Ribbon { anchors.centerIn: parent; width: 40; height: 200; vertical: true; candidate: version.index === 1 }
                    }
                    Column {
                        spacing: 10
                        Label { text: "Light panel · 240 × 48" }
                        Rectangle {
                            width: 280; height: 76; color: "#ebedf0"; radius: 8
                            Ribbon { anchors.centerIn: parent; width: 240; height: 48; backdropColor: "#ebedf0"; candidate: version.index === 1 }
                        }
                        Label { text: "Energy " + root.frame.energy.toFixed(2) + "    Bass " + root.frame.bass.toFixed(2) }
                        Label { text: "Mids " + root.frame.mid.toFixed(2) + "    Highs " + root.frame.treble.toFixed(2) }
                        Label {
                            text: version.index === 0 ? "Main path follows one advancing phase"
                                : "Arch " + root.frame.arch.toFixed(2) + "  Bend " + root.frame.counterBend.toFixed(2)
                        }
                        Label {
                            text: version.index === 0 ? "Phase " + root.frame.phase.toFixed(2)
                                : "Bias " + root.frame.bias.toFixed(2) + "  Opening " + root.frame.opening.toFixed(2)
                        }
                    }
                }
            }
        }
    }
    Row {
        x: 28; y: 748; spacing: 12
        CheckBox { text: "Isolate main shape"; checked: root.isolateShape; onToggled: root.isolateShape = checked }
        CheckBox { text: "Reduced motion"; checked: root.reduceMotion; onToggled: root.reduceMotion = checked }
        CheckBox { text: "Simple rendering"; checked: root.simpleRendering; onToggled: root.simpleRendering = checked }
        ComboBox { model: Palette.names(); currentIndex: root.paletteChoice; onActivated: root.paletteChoice = currentIndex; width: 130 }
    }
}
