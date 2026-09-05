# Independent mid-accent correction

Historical experiment. Its shape and mid-accent design are now integrated in the applet; see [shared motion integration](shared-motion.md). Statements below describe the original isolated pass.

The isolated main-shape prototype again responds to fast mid accents in both ShaderEffect and Canvas. This resolves the regression recorded in the [fine-motion review](vibration-review.md). The correction is in the prototype; it has not been installed into the Plasma widget.

| Before | After | Why |
| --- | --- | --- |
| An isolated mid accent changed no pixels when the slow shape and shared ripple stayed fixed. | A short additional counter-bend follows the existing `midAccent` envelope directly. | The response survives the shared ripple's cooldown and does not wait for the slow shape state. |
| The shader and fallback both omitted this response. | Both apply the same deformation along the current horizontally biased curve. | Keep timing, direction and size consistent between renderers. |

The extra bend is `0.024 * midAccent * counterBend`, using the existing smooth polynomial shape basis. It has no clock, extra filter, event queue or per-view animation state. The analyzer already supplies the short, smoothed accent envelope. The added centerline displacement is bounded below 0.024 of the view height for the prototype's valid shape range and normalized accents. It fades at the anchored ends, returns to zero with the accent and is suppressed in reduced motion. This restores a fast deformation without adding continuous tremor or changing the slower shape trajectory.

## Verification

The new optional `prototype-accents` suite loads the real `PrototypeRibbon.qml` and its compiled shader. It fixes band levels, phase, colors, the slow shape and shared onset, then changes only one accent. Image comparisons check visible geometry, bounded displacement, restrained light change, exact return to the resting image, reduced-motion invariance and complete silence transparency. Separate cases preserve bass and high responses.

- Before the fix, all ten mid-accent cases failed with zero changed channels. These cover both renderers at 160 × 32, 200 × 40, 240 × 48, 432 × 180 and a 40 × 200 vertical view.
- After the fix, the suite passed 16/16 at normal scale, 16/16 at 125%, and 16/16 with the software backend. Totals include QtTest setup and cleanup. With the software backend, both automatic and forced rendering cases exercise Canvas.
- With a fixed accent of 0.8 at 200 × 40, the GPU image had a peak alpha-centroid displacement of 0.764 logical pixels, and Canvas 0.797 pixels. Total RGB light changed by -0.18% and +0.23% respectively. The checks measure displayed geometry, not only a uniform reaching the shader.
- The corrected preview rebuilt successfully. QML lint produced no warnings.
- A six-second, 180-frame comparison uses generated PCM through the existing FFTW analyzer. It holds the displayed levels and slow shape fixed and disables the common ripple to isolate the mid-accent response. Ordered frames were inspected. The exported soundtrack contains the generated signal, not a recording of desktop audio.
- A fresh live preview connected to the default Sunshine output without dropped or expired blocks. That five-second check received silence, so it verifies connection and the empty state, not the correction's appearance during a real song.

The existing capture, analyzer, production shader and applet QML were not modified. Their older production test results remain historical; a complete backend/sanitizer rerun was not needed for this prototype shader/QML correction. Integration into the shared production motion state and long listening sessions remain separate work. The review's suggestions about band-specific shared ripples and overlap between repeated ripples are not part of this fix.

Verdict: **Approve this correction.** The independent mid response is restored in both renderers, with no residual deformation after the accent. The previously recorded mid-accent regression no longer blocks the prototype.

## Reproduce

From the repository root:

```sh
cmake -S . -B build-macro-motion-checks -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DLUMA_BUILD_MOTION_PROTOTYPE=ON -DLUMA_BUILD_PREVIEW=OFF -DBUILD_TESTING=ON
cmake --build build-macro-motion-checks --target test-prototype-accents -j 4
ctest --test-dir build-macro-motion-checks -R '^prototype-accents$' --output-on-failure
QT_SCALE_FACTOR=1.25 ctest --test-dir build-macro-motion-checks -R '^prototype-accents$' --output-on-failure
QT_QUICK_BACKEND=software ctest --test-dir build-macro-motion-checks -R '^prototype-accents$' --output-on-failure

# Interactive comparison on the current default audio output.
bash tools/macro-motion-prototype/run.sh --full
```

Local evidence is under `build/evidence/mid-accent-fix/`: `before.log`, `ctest.log`, `view.log`, `view-125.log`, `view-software.log`, build and lint logs, `live.log`, `live.png`, `trace.csv` and `mid-accent-correction.mp4`. The `before/` directory preserves the old view, shader and compiled shader. The temporary `Compare.qml`/`render.cpp` and resource manifest reproduce the visual diagnostic; `frames/` contains its captures. `final-sources.sha256` identifies the corrected sources.
