# Bloom control

Date: 2026-09-06. Baseline: `cbe9046`.

Bloom adjusts the soft light around the filaments from 0 to 150%, in steps of 5%. The default of 100% keeps the previous halo. At 0%, the filaments remain visible. Higher values strengthen the halo and veil without changing filament position, core width or audio response.

![Bloom at 0%, 100% and 150%](images/bloom-comparison.png)

Native 200 × 40 shader captures, shown at twice their size. The same analyzed synthetic mix is held in each image. Curvature, fullness, sensitivity, intensity and accents are at their upper limits to check the panel edges. No test sound is played through the desktop output.

| Before | After | Why |
| --- | --- | --- |
| The halo amount was fixed. | A Bloom slider with Off/Strong labels, a visible default and immediate preview. | Users can choose a sharper or softer ribbon independently of its shape and overall intensity. |
| Returning to a familiar appearance required adjusting each setting. | Reset appearance also restores Bloom to 100%. | A single action restores the visual defaults while keeping audio and accessibility preferences. |

The setting belongs to each widget and is shared by its panel, popup and configuration preview. Draft edits remain local to the preview until Apply. Cancel discards them. The simple renderer supports the same range, and both renderers still respect reduced motion and silence.

## Implementation

The shader scales its existing analytic halo and veil by the bounded bloom amount within the same rendering pass. The Canvas fallback scales only the opacity of the outer strokes and skips them entirely at 0%. Filament geometry, core terms, palette, capture and analysis remain unchanged. Non-finite settings fall back to 100%, while finite values are clamped to 0–150%.

The native applet exposes `appearanceRevision = 2`. The configuration page requires this revision before enabling appearance controls or their reset. This also detects a still-loaded plugin that supports curvature and fullness but has no Bloom setting or matching shader. The page explains that the update becomes available at the next login; it never restarts Plasma.

## Verification

- The build succeeds on CachyOS with Qt 6.11.2, Plasma 6.7.4 and GCC 16.2.1. All 10 CTest suites pass, including 65 view and 80 motion-view QtTest entries.
- View, motion-view and Plasma configuration checks also pass at 125% scaling.
- Software rendering passes 60 view entries with five intentional shader-only skips, all 80 motion-view entries and the Plasma configuration test.
- A compatibility fixture loads the installed pre-Bloom plugin with the new settings page. It confirms that the upgrade message and disabled controls work even though that plugin already exposes `previewSize`. The fixture passes with one Qt disconnect warning during teardown.
- QML lint completes with expected static warnings for custom native applet properties. The whitespace check passes.
- Installation under `/usr` succeeds. All 60 installed files match source/build, and the installed native plugin passes the Plasma integration test in a fresh process. The current desktop retains the previous library until the next login; no session or audio service was restarted.

The new rendered regression has 20 cases across hardware/simple rendering, light/dark palette variants, three horizontal panel sizes, a vertical panel and a popup. It checks 0%, 50%, 100% and 150%, monotonic emitted light, visible filaments at zero, clear outer edges, default restoration, invalid values, reduced motion and silence. Changing Bloom repaints a held snapshot without another audio sample or a change in shared shape.

The Plasma integration test checks the new KConfig default, draft initialization, mouse dragging, arrow keys, reset, persistence of a zero value, panel/popup propagation and independence from a second widget. The settings screenshot uses a held synthetic feature snapshot in a native configuration test window.

Local evidence is under `build/evidence/bloom-control/`. The repeatable commands are:

```sh
cmake --build build -j 4
ctest --test-dir build --output-on-failure
QT_SCALE_FACTOR=1.25 ctest --test-dir build -R '^(view|motion-view|plasma)$' --output-on-failure
QT_QUICK_BACKEND=software ctest --test-dir build -R '^(view|motion-view|plasma)$' --output-on-failure
```

Long listening sessions and hardware output changes are outside this visual control change and were not repeated.
