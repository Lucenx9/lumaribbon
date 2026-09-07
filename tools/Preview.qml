// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import "../package/contents/ui"
import "../package/contents/ui/Palette.js" as Palette

Rectangle {
    id: root
    required property var previewAudio
    required property bool syntheticInput
    required property bool simpleRendering
    required property bool reducedPreview
    required property real referenceTime
    readonly property bool usingFallback: simpleRendering || GraphicsInfo.api === GraphicsInfo.Software
    width: 860; height: 720
    color: "#13171e"
    component Sample: RibbonView {
        audio: root.previewAudio
        forceFallback: root.simpleRendering
        reducedMotion: root.reducedPreview
    }
    Column {
        anchors.fill: parent
        anchors.margins: 30
        spacing: 18
        Text { text: "Luma Ribbon"; color: "#eff3f5"; font.pixelSize: 26 }
        Text {
            text: (root.syntheticInput ? "Synthetic signals analyzed with FFTW" : "Default PipeWire output monitor")
                + (root.usingFallback ? " · simple rendering" : " · Qt Quick shader")
                + (root.referenceTime >= 0 ? " · frozen at " + root.referenceTime.toFixed(2) + " s" : "")
            color: "#a0aaba"; font.pixelSize: 13
        }
        Grid {
            columns: 3
            columnSpacing: 24
            rowSpacing: 16
            Repeater {
                model: Palette.names()
                Column {
                    id: sampleColumn
                    required property string modelData
                    required property int index
                    spacing: 8
                    Text { text: sampleColumn.modelData + " · 120 × 40"; color: "#a0aaba"; font.pixelSize: 12 }
                    Rectangle {
                        width: 224; height: 64; radius: 8; color: "#232830"
                        Sample { anchors.centerIn: parent; width: 120; height: 40; paletteIndex: sampleColumn.index }
                    }
                }
            }
        }
        Row {
            spacing: 24
            Rectangle {
                width: 688; height: 240; radius: 12; color: "#1b2028"
                Text { x: 20; y: 16; text: "Expanded view · same effect, same analysis"; color: "#8491a4"; font.pixelSize: 12 }
                Sample { anchors.centerIn: parent; width: 640; height: 190 }
            }
            Rectangle {
                width: 64; height: 240; radius: 8; color: "#232830"
                Sample { anchors.centerIn: parent; width: 40; height: 120; vertical: true }
            }
        }
        Row {
            spacing: 24
            Rectangle {
                width: 224; height: 64; radius: 8; color: "#ebedf0"
                Sample { anchors.centerIn: parent; width: 120; height: 40; backdropColor: "#ebedf0" }
            }
            Rectangle {
                width: 184; height: 64; radius: 8; color: "#232830"
                Sample { anchors.centerIn: parent; width: 80; height: 32 }
            }
            Rectangle {
                width: 264; height: 64; radius: 8; color: "#232830"
                Sample { anchors.centerIn: parent; width: 160; height: 48 }
            }
        }
        Text { text: "Transparency on a light background · small panel · large panel"; color: "#8491a4"; font.pixelSize: 12 }
    }
}
