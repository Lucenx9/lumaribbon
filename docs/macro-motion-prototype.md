# Audio-shaped main curve

Historical experiment. Its shape and mid-accent design are now integrated in the applet; see [shared motion integration](shared-motion.md). Statements below describe the original isolated pass.

Follow-up status: the [fine-motion review](vibration-review.md) identified a missing fast mid-accent deformation. The [correction](mid-accent-fix.md) now restores that response in both renderers and verifies it at panel size, 125% scale and in software. This resolves that regression; the prototype still awaits production integration.

Date: 2026-09-05. This follows the [main-shape review](macro-motion-review.md). Apple Design and Emil Design Engineering guide continuity, restrained motion and legibility at panel size. Prototype supplies the isolated comparison workflow, adapted to native Qt and the agreed single candidate. Karpathy Guidelines and Codebase Design keep the experiment outside the production audio path.

The runnable study is in [tools/macro-motion-prototype](../tools/macro-motion-prototype/README.md). It is an optional executable, not an installed widget update.

## Design result

| Before | After in the candidate | Why |
| --- | --- | --- |
| Two main waves follow one advancing phase. Audio mostly changes their height and speed. | Four audio-driven values control the broad arch, counter-bend, horizontal bias and filament opening. | Bass, mid and high passages can produce different silhouettes at the same phase. |
| Filament opening follows another phase-driven fold. | The bundle opens around the current audio-shaped curve. | The filaments move with the main shape rather than repeatedly scissoring through it. |
| Bass-light passages become nearly straight. | Mids can produce a clear counter-bend and high passages a shallow opposite arch. | The main shape remains readable when bass energy is low. |
| The first prototype became too straight with a balanced mix. | A small counter-bend remains while audible, with an additional contribution from mids. | Avoid cancellation between the broad curves without making the effect faster or noisier. |

Verdict: **Approve the candidate as a design direction for further listening and production integration.** The rendered sequences show different silhouettes and continuous transitions. The strongest improvement is at changes of timbre or musical phrase. A passage with stable timbre settles into a relatively stable shape. This is intentional; adding random wandering would weaken the relationship to audio. Preference for that calmer behavior still needs evaluation over complete songs.

The candidate uses relative band envelopes and the difference between current energy/mids and slower memories of those signals. Four critically damped spring states move toward those targets from their current positions and velocities. There are no random targets, shape presets or periodic shape switches. A small procedural displacement provides continuity, bounded below 0.72 logical pixels at 40 px height for normalized energy. Existing local attack ripples and colors remain available.

Both renderers use the same candidate centerline formula. Reduced motion holds the shape controls and procedural phase fixed, while sustained audio can still change body and opacity. The shape stops updating below the audible threshold and the existing energy envelope fades it out. Three bands cannot identify individual instruments or extract a melody; the response remains a visualization of the mixture.

## Executed checks

Evidence is local under `build/evidence/macro-motion-prototype/`. Captures come from the actual Qt Quick shaders and Canvas, not generated artwork.

- Built the optional target with the repository's C++/Qt/PipeWire/FFTW dependencies. Qt's shader tools compiled both shaders. The default project build also succeeded, with `LUMA_BUILD_MOTION_PROTOTYPE=OFF`.
- QML lint passed without warnings after removing a property name that shadowed `QQuickItem.palette`.
- Captured 780 frames at 30 samples per second for a 26-second synthetic study. The analysis is generated from PCM through the actual analyzer. Inspected ordered frames for bass, mids, highs, a changing blend, quiet input and the final fade.
- Recorded the complete effect separately with colors and attacks enabled. Its 780-row feature/shape trace is byte-identical to the isolated study. Both exported videos are 26 seconds at 30 FPS, with H.264 video and the generated signal encoded as AAC audio.
- Checked 200 × 40, 160 × 32, 240 × 48, an expanded view and a 40 × 200 vertical view. These are fixtures in one native window, not a modified desktop panel or popup.
- Inspected a 125% capture at 1250 × 1000 physical pixels for the 1000 × 800 logical window, including full attacks and dynamic colors. Rendered reduced-motion Ice, forced-fallback Ember and automatic software fallback. Both dark and light backgrounds are in every capture.
- Reconnected the standalone study to the current default `sink-sunshine-stereo` monitor. A first capture had almost no signal; a later five-second capture had audible Spotify input, with zero dropped and expired blocks. After startup, 124 displayed snapshots had energy 0.576–0.757, arch 0.089–0.377, counter-bend 0.301–0.715 and horizontal bias -0.329–0.187. These ranges demonstrate actual audio-driven shape variation, not musical quality by themselves. Only features and an image were saved from live input, not the song audio.
- After the finite live check, only the desktop widget's Luma monitor remained. The user's Plasma process stayed at PID 264298. This work did not restart desktop or audio services, change routing or operate the player.

A final six-second run of the existing `luma-audio-probe --seconds 6 --expect-audio` also succeeded at stereo 48 kHz on Sunshine, with no errors, drops or expired blocks. It reported `shared-engine=yes released=yes`; its log is `live-probe.log`. This checks the existing capture module used by the study, not production integration of the candidate motion state.

The synthetic trace contains 780 finite snapshots. At 201 horizontal points per frame, the candidate centerline stayed between 0.314 and 0.696 of the view height. Its largest adjacent-frame movement was 0.803 logical pixels at 40 px height, around the deliberate abrupt input change at 12.23 seconds. This calculation excludes local attack ripples and filament glow; it is a shape-continuity diagnostic, not a universal clipping or performance guarantee. Energy decayed to 0.000185 at the end, and the shape remained fixed below its gate.

At fixed phase, fitting each sampled curve as a scalar multiple of another left relative residuals between 0.238 and 0.987 across the four sampled passages. Together with the images, this supports that the candidate changes curve shape rather than just amplitude. The numerical diagnostics use the same centerline expression evaluated from the recorded state.

Files:

- `main-shape-comparison.mp4`: isolated shape comparison with fixed palette and suppressed short attacks. The soundtrack is the generated test signal.
- `full-effect-comparison.mp4`: the same sequence with dynamic colors and attacks enabled.
- `shape-sequence.png`: ordered shape comparison at 2.5, 6, 10.5 and 14.5 seconds, current on the left and candidate on the right.
- `live-final.png`, `live-final.csv`, `live-final.log`, `live-metrics.json`: the brief real-input check.
- `fractional-125.png`, `reduced-ice.png`, `fallback-ember.png`, `software.png`: rendering modes and scaling.
- `metrics.json`, `final-isolated/trace.csv`, `final-full/trace.csv`: measured state and continuity.
- `build-final.log`, `default-configure.log`, `default-build.log`, `qmllint.log`, `final-sources.sha256`: build and source evidence.

## Scope before integration

The installed shader, audio classes and applet QML are unchanged. Existing production test and sanitizer results remain those recorded in [verification](verification.md); they were not rerun as a new claim for this disposable candidate. Its default build isolation and native rendering were checked here.

A production implementation still needs shared worker-owned motion state, exposure through `AudioSnapshot` and `AudioState`, and integration with the real `RibbonView`. It also needs sustained-tone and changing-input checks for the motion state, reduced-motion invariance, hidden-view/late-popup consistency, multiple instances and normal resource teardown. Long listening sessions, GPU frame-pacing measurements for the new shader, moving the actual panel across differently scaled displays and final desktop settings validation remain outside this prototype pass.
