# Grove refinement

Date: 2026-09-06. Baseline: `228245b`.

Grove maintains a distinct leaf-green identity as the mix becomes brighter, without drifting into yellow-green lime. The primary colors are unchanged, so bass-heavy passages keep their jade green tone. The adjustment applies to both fixed and audio-reactive modes across both the shader and canvas renderers. In addition, `createSpectrum` defaults omitted or non-finite degree offsets to zero, preventing `NaN` spectrum stops.

| Before | After | Why |
| --- | --- | --- |
| Grove's secondary color approached yellow-green lime in bright passages. | Its secondary color is a foliage leaf green, with a pale green highlight. | Keep a stronger green identity through treble passages without washing out to yellow. |
| `createSpectrum` evaluated to `NaN` when `degrees` was omitted or undefined. | `degrees` safely falls back to `0` when not a finite number. | Keep the spectrum valid when called without explicit degree rotation. |

## Implementation

Grove's color triplets in `Palette.js` update to keep the secondary and highlight colors in the green family (OKLCH hue ~140°, HSV hue ~109°). In `createSpectrum`, the angle computation guards against non-finite or omitted arguments.

| Palette / theme | Primary | Secondary | Highlight |
| --- | --- | --- | --- |
| Grove / dark | `#45b89a` | `#98e088` | `#e8ffe3` |
| Grove / light | `#16724f` | `#529543` | `#afdfa5` |

## Verification

The held renderer fixture in `test_view.cpp` tests sustained treble and a treble accent together, measuring the alpha-weighted mean color of visible pixels. Grove now requires the highlighted body to remain in a leaf-green hue range (`105°` to `155°` in HSV). All four Grove cases failed with the former colors and pass with the refined colors.

| Fixture | Before hue | After hue | Before / after Oklab chroma |
| --- | --- | --- | --- |
| Grove / shader dark | 88.84° | 115.29° | 0.121 / 0.118 |
| Grove / shader light | 81.14° | 113.44° | 0.127 / 0.125 |
| Grove / canvas dark | 88.23° | 114.44° | 0.123 / 0.124 |
| Grove / canvas light | 81.64° | 114.34° | 0.129 / 0.126 |

All 10 CTest suites pass (`analysis`, `musical-response`, `color-response`, `motion`, `engine-recovery`, `engine-unexpected-recovery`, `audio-state`, `view`, `motion-view`, `plasma`).
