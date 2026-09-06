// SPDX-License-Identifier: GPL-3.0-or-later
.pragma library

// Stable saved indices: append new palettes without reordering existing ones.
function names() {
    return ["Aurora", "Ember", "Ice", "Grove", "Iris", "Coral", "Hue"];
}
function colors(index, light) {
    switch (index) {
    case 1:
        return light ? ["#9f1822", "#be3f1b", "#d9782d"] : ["#d62532", "#ff6530", "#ff9a3d"];
    case 2:
        return light ? ["#4568b7", "#257a9c", "#8acbdf"] : ["#6489d9", "#82d8ed", "#effcff"];
    case 3:
        return light ? ["#16724f", "#718f24", "#c5d994"] : ["#45b89a", "#b3da72", "#ecffd7"];
    case 4:
        return light ? ["#6140a8", "#a84da9", "#d2aae3"] : ["#8060cc", "#d58cdf", "#f8e6ff"];
    case 5:
        return light ? ["#b7467b", "#c8753b", "#efb6a2"] : ["#e879aa", "#ffab79", "#ffe4df"];
    default:
        return light ? ["#6955b7", "#188778", "#93d5be"] : ["#9278e8", "#59d7bf", "#d7fff0"];
    }
}

// Fraction of the ramp that audio-reactive color may span. Light-theme Ice
// keeps a narrow spread so small timbre changes stay legible in the simple
// renderer.
function spreadLimit(index, light) {
    return index === 2 && light ? 0.08 : 0.5;
}

// Oklab matrices from Bjorn Ottosson's public-domain reference implementation:
// https://bottosson.github.io/posts/oklab/ (2021-01-25 matrices).
// Conversion and gamut mapping happen when palette, theme or hue changes.
// Views retain one 65-color ramp; audio-frame sampling only interpolates.
function linear(x) {
    return x <= 0.04045 ? x / 12.92 : Math.pow((x + 0.055) / 1.055, 2.4);
}
function encoded(x) {
    x = Math.max(0, Math.min(1, x));
    return x <= 0.0031308 ? 12.92 * x : 1.055 * Math.pow(x, 1 / 2.4) - 0.055;
}
function toLch(color) {
    const r = linear(color.r), g = linear(color.g), b = linear(color.b);
    const l = Math.cbrt(0.4122214708*r + 0.5363325363*g + 0.0514459929*b);
    const m = Math.cbrt(0.2119034982*r + 0.6806995451*g + 0.1073969566*b);
    const s = Math.cbrt(0.0883024619*r + 0.2817188376*g + 0.6299787005*b);
    const a = 1.9779984951*l - 2.4285922050*m + 0.4505937099*s;
    const blue = 0.0259040371*l + 0.7827717662*m - 0.8086757660*s;
    return [0.2104542553*l + 0.7936177850*m - 0.0040720468*s,
        Math.sqrt(a*a + blue*blue), Math.atan2(blue, a)];
}
function toLinearRgb(lightness, chroma, hue) {
    const a = chroma * Math.cos(hue), b = chroma * Math.sin(hue);
    const l = Math.pow(lightness + 0.3963377774*a + 0.2158037573*b, 3);
    const m = Math.pow(lightness - 0.1055613458*a - 0.0638541728*b, 3);
    const s = Math.pow(lightness - 0.0894841775*a - 1.2914855480*b, 3);
    return [4.0767416621*l - 3.3077115913*m + 0.2309699292*s,
        -1.2684380046*l + 2.6097574011*m - 0.3413193965*s,
        -0.0041960863*l - 0.7034186147*m + 1.7076147010*s];
}
function inGamut(rgb) {
    return rgb[0] >= 0 && rgb[0] <= 1 && rgb[1] >= 0 && rgb[1] <= 1 && rgb[2] >= 0 && rgb[2] <= 1;
}
function fromLch(lightness, chroma, hue) {
    let rgb = toLinearRgb(lightness, chroma, hue);
    if (!inGamut(rgb)) {
        // Reduce chroma, retaining lightness and hue. Fixed work avoids
        // clipping RGB channels into a different hue.
        let low = 0, high = chroma;
        for (let iteration = 0; iteration < 12; ++iteration) {
            const candidate = (low + high) * 0.5;
            if (inGamut(toLinearRgb(lightness, candidate, hue))) low = candidate;
            else high = candidate;
        }
        rgb = toLinearRgb(lightness, low, hue);
    }
    return Qt.rgba(encoded(rgb[0]), encoded(rgb[1]), encoded(rgb[2]), 1);
}
function createSpectrum(light, degrees) {
    // One complete color wheel across the ribbon, with an even perceived
    // lightness. Cache these six anchors and the matching final endpoint.
    const angle = degrees === 180 ? -180 : degrees;
    const stops = [];
    for (let i = 0; i < 6; ++i)
        stops.push(fromLch(light ? 0.52 : 0.73, 0.14, (300 - i * 60 + angle) * Math.PI / 180));
    stops.push(stops[0]);
    return stops;
}
function createRamp(first, last) {
    const a = toLch(first), b = toLch(last);
    let hueDelta = b[2] - a[2];
    if (hueDelta > Math.PI) hueDelta -= 2 * Math.PI;
    if (hueDelta < -Math.PI) hueDelta += 2 * Math.PI;
    const ramp = [first];
    for (let i = 1; i < 64; ++i) {
        const t = i / 64;
        const lightness = a[0] + (b[0] - a[0]) * t;
        const chroma = a[1] + (b[1] - a[1]) * t;
        const hue = a[2] + hueDelta * t;
        ramp.push(fromLch(lightness, chroma, hue));
    }
    ramp.push(last);
    return ramp;
}
function sample(ramp, position) {
    const last = ramp.length - 1;
    const x = Math.max(0, Math.min(1, position)) * last;
    const index = Math.min(last - 1, Math.floor(x));
    const t = x - index, a = ramp[index], b = ramp[index + 1];
    return Qt.rgba(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1);
}
