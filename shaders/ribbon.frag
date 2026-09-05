// SPDX-License-Identifier: GPL-3.0-or-later
#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float intensity;
    vec2 resolution;
    vec4 bands; // energy, bass, mid, treble
    vec4 motion; // phase, attack envelope, ripple age, reduced motion
    vec4 accents; // bass, mid, treble transients; latched ripple origin
    vec4 shape; // arch, counter-bend, horizontal bias, opening
    vec4 colorA;
    vec4 colorB;
    vec4 colorC;
};

float gaussian(float distance, float width) {
    float d = distance / max(width, 0.0001);
    return exp(-d * d);
}
float filamentWeight(float f) {
    return 1.0 - 0.14 * abs(f) - 0.055 * f * f;
}
void main() {
    vec2 uv = qt_TexCoord0;
    float x = uv.x;
    float quiet = smoothstep(0.001, 0.13, bands.x);
    float edge = smoothstep(0.0, 0.095, x) * smoothstep(0.0, 0.095, 1.0 - x);
    // Let faint halo tails dissolve before the panel clips them at high sensitivity.
    float verticalEdge = smoothstep(0.0, 0.065, uv.y) * smoothstep(0.0, 0.065, 1.0 - uv.y);
    float envelope = pow(max(sin(x * 3.141593), 0.0), 0.72);
    float reduced = motion.w;
    // Keep time coefficients in integer hundredths: the engine wraps at 200*pi.
    // A branch keeps the fixed phase bit-stable instead of interpolating time.
    float t = reduced > 0.5 ? 0.65 : motion.x;
    // The broad curve is shaped by audio, not transported by a clock.
    vec4 form = reduced > 0.5 ? vec4(0.25, 0.25, 0.0, 0.4) : shape;
    float u = x + form.z * 0.65 * x * (1.0 - x);
    float arch = 4.0 * u * (1.0 - u);
    float counterBend = 2.5 * arch * (2.0 * u - 1.0);
    float body = (0.13 + 0.065 * bands.x + 0.02 * bands.y) * mix(1.0, 0.35, reduced);
    float bend = -form.x * arch + form.y * counterBend;
    // Keep fast mid attacks independent of the slow shape and shared ripple.
    // Match the Canvas expression. Peak displacement is < 0.024 of the height.
    float midAccentBend = 0.024 * accents.y * counterBend * (1.0 - reduced);
    // Less than one logical pixel at 40 px height. Only continuity is procedural.
    float continuity = 0.018 * bands.x * sin(x * 6.283185 - t * 0.73) * envelope;
    // A rounded distance keeps attack ripples smooth where the two wavefronts meet.
    float originDistance = x - accents.w;
    float wavefront = sqrt(originDistance * originDistance + 0.0016) - 0.04 - motion.z * 0.7;
    float ripple = sin(wavefront * 48.0) * gaussian(wavefront, 0.14)
                 * motion.y * 0.032 * (1.0 - reduced);
    float center = 0.51 + body * bend + continuity + midAccentBend + ripple;
    float thickness = 0.032 + 0.055 * bands.y + 0.014 * accents.x;
    float pixel = 1.0 / max(resolution.y, 1.0);
    // Narrow the bundle as well as fading it, so the ends never form a blunt cap.
    float taper = mix(0.48, 1.0, smoothstep(0.0, 0.16, x) * (1.0 - smoothstep(0.82, 1.0, x)));
    vec3 sum = vec3(0.0);
    float density = 0.0;
    float strands[5];
    float veilCenter = 0.0;
    float totalWeight = 0.0;
    for (int i = 0; i < 5; ++i) {
        float f = float(i) - 2.0;
        float drift = sin(x * (7.0 + float(i) * 0.38) + t * (0.55 + float(i) * 0.09) + f * 0.8);
        float fold = f * thickness * (0.32 + 0.88 * form.w)
            * (0.24 + 0.76 * sin(u * 3.141593 + f * 0.32 + form.y * 0.35));
        strands[i] = center + (fold + drift * (0.012 + bands.z * 0.012) * envelope
            + bands.w * 0.004 * sin(x * 37.0 + f * 2.0 - t) * (1.0 - reduced)) * taper;
        veilCenter += strands[i] * filamentWeight(f);
        totalWeight += filamentWeight(f);
    }

    // Center the weighted bundle on the audio curve. Asymmetric folds must not
    // shift the whole bundle in the opposite direction and flatten that curve.
    float bundleOffset = veilCenter / totalWeight - center;
    vec3 ribbonColor = mix(colorA.rgb, colorB.rgb, smoothstep(0.08, 0.92, x));
    float veil = gaussian(uv.y - center, (thickness * 1.8 + pixel) * taper);
    sum += veil * ribbonColor * 0.18;
    density += veil * 0.18;

    for (int i = 0; i < 5; ++i) {
        float f = float(i) - 2.0;
        float distance = uv.y - (strands[i] - bundleOffset);
        float coreWidth = max(pixel * 0.5, (0.004 + 0.003 * bands.y) * (1.0 - 0.08 * abs(f)) * taper);
        float core = gaussian(distance, coreWidth);
        float halo = gaussian(distance, (thickness * 0.62 + pixel) * taper);
        float glint = accents.z * pow(0.5 + 0.5 * sin(x * 18.0 + f * 1.7 - t), 4.0);
        vec3 tint = mix(ribbonColor, colorC.rgb, core * (0.14 + bands.w * 0.17 + glint * 0.2));
        float light = (halo * 0.14 + core * (0.48 + glint * 0.12)) * filamentWeight(f);
        sum += tint * light;
        density += light;
    }
    float coverage = quiet * edge * (0.35 + 0.65 * sqrt(bands.x)) * intensity;
    coverage = clamp(coverage, 0.0, 1.0) * verticalEdge;
    float alpha = (1.0 - exp(-density * 1.65)) * clamp(coverage, 0.0, 1.0);
    vec3 rgb = sum / max(density, 0.001);
    // ShaderEffect requires premultiplied output, including qt_Opacity.
    fragColor = vec4(rgb * alpha, alpha) * qt_Opacity;
}
