# Main-shape motion review

Follow-up: the proposed isolated comparison has now been implemented and evaluated in the [main-shape prototype report](macro-motion-prototype.md). This document preserves the findings before that experiment.

Date: 2026-09-05. Scope: the ribbon's large-scale silhouette and motion, excluding attack ripples and fine vibration. The review applies Apple Design, Emil Design Engineering and Review Animations to the existing Qt Quick shader. No rendering, capture or analysis implementation was changed during this review.

| Before | Proposed after | Why |
| --- | --- | --- |
| The centerline combines two fixed spatial sine waves. Audio primarily scales their amplitude, and mids modestly change the second wave's weight. See [ribbon.frag:35](../shaders/ribbon.frag#L35). | Let audio control a small set of smooth shape parameters, such as crest position, broad curvature and asymmetry. Blend continuously within one ribbon effect. | Different musical passages need to change the silhouette as well as its size. |
| One monotonically advancing phase drives both the main bend and filament folds. Audio changes its speed, but the geometry still follows the same family of paths. See [AudioEngine.cpp:55](../src/AudioEngine.cpp#L55) and [ribbon.frag:55](../shaders/ribbon.frag#L55). | Give the shared motion state independently smoothed curvature and opening targets derived from audio. Retain a smaller procedural motion for continuity. | Replacing one repeated sine with several unrelated oscillators would add variety without a clearer musical relationship. |
| Sustained mids and especially treble have little large-scale displacement. With bass/mid envelopes near zero and accents suppressed, centerline displacement is bounded by about 1.46 logical pixels at 40 px height. | Keep a legible main shape while audio is audible, with bass adding body and other bands influencing tension and where bends form. Respect the same panel bounds. | Quiet bass does not have to make the whole ribbon look nearly flat during a vocal or high-register passage. |

Origin, physicality and cohesion: the user's description is supported by the code and the rendered diagnostic. The ribbon is not motionless, and its complete phase cycle is not a short repeated clip. Its movement vocabulary is narrow enough that different passages often look like the same wave travelling at a different speed or height. The recent timing and spectral-origin update changes local attacks, not this centerline model.

The proposed direction is a continuous ribbon that opens, stretches and changes its broad bends with the audio. Its state should evolve from the current shape, with inertia across musical changes, and remain shared by panel and popup. There should be no random direction changes, added presets, switching between named shapes, increased flashing or faster vibration as a substitute for shape development. Reduced motion and silence behavior must remain intact.

Verdict: **Block for the large-scale motion design against the intended aurora-like identity.** This is a visual design finding; it does not invalidate the passing capture, stability or lifecycle checks. The next step should be one isolated shape prototype compared against the current shader with identical analyzed input at 200 × 40, 160 × 32, expanded and vertical sizes. The proposal has not been implemented or visually validated yet.

## Diagnostic

`build/evidence/macro-motion-review/study.png` is an actual render of the current shader at panel size. Rows receive equal-amplitude 94, 740 and 4800 Hz sine waves analyzed through the current FFTW implementation for two seconds at 48 kHz. Columns hold phase at 0, 1.6, 3.2 and 4.8. The palette is fixed and attack envelopes are suppressed to isolate the main shape. Sustained band response and filament rendering remain active.

All three inputs produce normalized energy near 0.755. Their dominant band is near 0.754, while the other bands are near zero. The capture shows that phase selects broadly similar paths across rows, while the audio changes thickness and amplitude much more. It is a controlled shape diagnostic, not a claim about the exact appearance of a complete song or a measured user-perception score. Its source and log are alongside the image.
