// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import "../../package/contents/ui/Palette.js" as Palette

Item {
    id: root
    property bool candidate: false
    property int paletteIndex: 0
    property bool dynamicColor: true
    property real intensity: 1.0
    property bool reducedMotion: false
    property bool forceFallback: false
    property bool vertical: false
    // Nominal host background, not a sampled screen pixel. No opaque backing is drawn.
    property color backdropColor: "#20242c"
    readonly property bool lightBackground: 0.2126 * backdropColor.r + 0.7152 * backdropColor.g
        + 0.0722 * backdropColor.b > 0.55
    readonly property bool software: GraphicsInfo.api === GraphicsInfo.Software
    readonly property bool fallback: forceFallback || software || shaderFailed
    property bool shaderFailed: false
    readonly property Canvas fallbackCanvas: fallbackLoader.item as Canvas
    property var frame: ({ energy: 0, bass: 0, mid: 0, treble: 0, onset: 0, phase: 0, rippleAge: 10,
        bassAccent: 0, midAccent: 0, trebleAccent: 0 })
    // Missing accent fields remain compatible with an already-loaded older plugin.
    readonly property vector3d accents: reducedMotion ? Qt.vector3d(0, 0, 0)
        : Qt.vector3d(frame.bassAccent || 0, frame.midAccent || 0, frame.trebleAccent || 0)
    readonly property var paletteColors: Palette.colors(paletteIndex, lightBackground)
    readonly property color primaryColor: paletteColors[0]
    readonly property color secondaryColor: paletteColors[1]
    readonly property color highlightColor: paletteColors[2]
    readonly property var paletteRamp: Palette.createRamp(primaryColor, secondaryColor)
    // Timbre is smoothed once in the shared analyzer, not by per-view animations.
    // Older loaded plugins have no timbre fields and retain their fixed gradient.
    readonly property bool colorActive: dynamicColor && frame.spectralBalance !== undefined
    // Give mixed music a visible hue range without cycling beyond the palette.
    readonly property real colorPosition: colorActive
        ? Math.max(0, Math.min(1, (frame.spectralBalance - 0.3) / 0.6)) : 0.5
    readonly property real colorBalance: colorPosition * colorPosition * (3 - 2 * colorPosition)
    readonly property real colorHighlight: colorActive ? (frame.trebleShare || 0) : 0
    // Open the gradient around the same timbre-selected center. Mixed passages
    // span up to 41% of the ramp. Light-theme Ice uses 20% to retain small
    // timbre changes in the simple renderer. Dominant hues remain unchanged.
    readonly property real colorSpread: (paletteIndex === 2 && lightBackground ? 0.08 : 0.5)
        * colorBalance * (1 - colorBalance)
    readonly property color startColor: colorActive
        ? mixColor(Palette.sample(paletteRamp, 0.84 * colorBalance - colorSpread), highlightColor, 0.08 * colorHighlight)
        : primaryColor
    readonly property color endColor: colorActive
        ? mixColor(Palette.sample(paletteRamp, 0.16 + 0.84 * colorBalance + colorSpread), highlightColor, 0.16 * colorHighlight)
        : secondaryColor
    readonly property real strength: Math.max(0.4, Math.min(1.6, intensity))
    clip: true

    function mixColor(a: color, b: color, amount: real): color {
        return Qt.rgba(a.r + (b.r - a.r) * amount, a.g + (b.g - a.g) * amount,
            a.b + (b.b - a.b) * amount, 1);
    }
    onFrameChanged: if (fallbackCanvas) fallbackCanvas.requestPaint()

    onStartColorChanged: if (fallbackCanvas) fallbackCanvas.requestPaint()
    onEndColorChanged: if (fallbackCanvas) fallbackCanvas.requestPaint()
    onFallbackChanged: {
        if (fallbackCanvas) fallbackCanvas.requestPaint();
    }
    onReducedMotionChanged: if (fallbackCanvas) fallbackCanvas.requestPaint()

    Item {
        id: ribbon
        anchors.centerIn: parent
        width: root.vertical ? root.height : root.width
        height: root.vertical ? root.width : root.height
        rotation: root.vertical ? -90 : 0
        visible: root.frame.energy > 0

        Loader {
            anchors.fill: parent
            active: !root.fallback
            sourceComponent: ShaderEffect {
                property real intensity: root.strength
                property vector2d resolution: Qt.vector2d(width, height)
                property vector4d bands: Qt.vector4d(root.frame.energy, root.frame.bass, root.frame.mid, root.frame.treble)
                property vector4d motion: Qt.vector4d(root.frame.phase, root.frame.onset, root.frame.rippleAge, root.reducedMotion ? 1 : 0)
                property vector4d accents: Qt.vector4d(root.accents.x, root.accents.y, root.accents.z,
                    root.frame.rippleOrigin === undefined ? 0.46 : root.frame.rippleOrigin)
                property vector4d shape: Qt.vector4d(root.frame.arch || 0, root.frame.counterBend || 0,
                    root.frame.bias || 0, root.frame.opening || 0.3)
                property color colorA: root.startColor
                property color colorB: root.endColor
                property color colorC: root.highlightColor
                fragmentShader: root.candidate ? "qrc:/prototype/ribbon-prototype.frag.qsb" : "qrc:/prototype/ribbon-legacy.frag.qsb"
                onStatusChanged: if (status === ShaderEffect.Error) root.shaderFailed = true
            }
        }
        Loader {
            id: fallbackLoader
            anchors.fill: parent
            active: root.fallback
            onLoaded: if (root.fallbackCanvas) root.fallbackCanvas.requestPaint()
            sourceComponent: Canvas {
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.clearRect(0, 0, width, height);
                    const e = root.frame.energy;
                    if (e < 0.001) return;
                    const phase = root.reducedMotion ? 0.65 : root.frame.phase;
                    const amplitude = (0.035 + 0.12 * root.frame.bass + 0.06 * root.frame.mid
                        + 0.03 * root.accents.x) * (root.reducedMotion ? 0.35 : 1);
                    const gradient = ctx.createLinearGradient(0, 0, width, 0);
                    gradient.addColorStop(0, "transparent");
                    gradient.addColorStop(0.13, root.startColor);
                    gradient.addColorStop(0.7, root.endColor);
                    gradient.addColorStop(1, "transparent");
                    ctx.lineCap = "round";
                    ctx.strokeStyle = gradient;
                    // Faint nested strokes soften the halo without an expensive blur.
                    const glowWidth = height * (0.17 + 0.11 * root.frame.bass + 0.04 * root.accents.x);
                    for (let layer = 0; layer < 8; ++layer) {
                        const depth = layer / 7;
                        ctx.globalAlpha = Math.min(1, Math.sqrt(e) * root.strength)
                            * (layer === 7 ? 0.64 + 0.1 * root.accents.z : 0.018 + 0.075 * depth * depth);
                        ctx.lineWidth = layer === 7 ? Math.max(1.2, height * 0.018)
                            : glowWidth * (1 - depth * 0.88);
                        ctx.beginPath();
                        for (let i = 0; i <= 64; ++i) {
                            const x = i / 64;
                            const curve = Math.sin(x * 6.283185 - phase * 1.13) * 0.68
                                + Math.sin(x * 12.56637 + phase * 0.73 + 1.3) * (0.28 + 0.18 * root.accents.y);
                            let y = height * (0.51 + amplitude * curve * Math.sin(x * Math.PI));
                            if (root.candidate) {
                                const bias = root.reducedMotion ? 0 : (root.frame.bias || 0);
                                const archAmount = root.reducedMotion ? 0.25 : (root.frame.arch || 0);
                                const counter = root.reducedMotion ? 0.25 : (root.frame.counterBend || 0);
                                const u = x + bias * 0.65 * x * (1 - x);
                                const arch = 4 * u * (1 - u);
                                const body = (0.13 + 0.065 * e + 0.02 * root.frame.bass)
                                    * (root.reducedMotion ? 0.35 : 1);
                                const continuity = 0.018 * e * Math.sin(x * 6.283185 - phase * 0.73)
                                    * Math.pow(Math.max(0, Math.sin(x * Math.PI)), 0.72);
                                // Match the shader's fast bend; reduced motion zeros accents.
                                const midAccentBend = 0.024 * root.accents.y * 2.5 * arch * (2 * u - 1);
                                y = height * (0.51 + body * (-archAmount * arch
                                    + counter * 2.5 * arch * (2 * u - 1)) + continuity + midAccentBend);
                            }
                            if (i === 0) ctx.moveTo(x * width, y); else ctx.lineTo(x * width, y);
                        }
                        ctx.stroke();
                    }
                }
            }
        }
    }
}
