# Fine motion and attack review

Resolved follow-up: the [mid-accent correction](mid-accent-fix.md) restores the prototype's independent mid response in both renderers and adds regression checks. The findings and verdict below preserve the pre-fix review; its specific integration blocker is now resolved.

Date: 2026-09-05. Scope: local ripples, band accents and fine filament movement in the current production sources and the isolated main-shape prototype. This is a review, with no renderer or analyzer changes. Review Animations and Emil Design Engineering supply the review method. Browser-specific interaction rules are not applied literally to this audio-driven Qt shader.

## Findings

| Before | Proposed after | Why |
| --- | --- | --- |
| The prototype's new centerline does not use `accents.y`. Changing the mid accent alone produces an identical image, in both ShaderEffect and Canvas. The production centerline uses it in the second bend. | Restore a short mid-accent deformation independently of the slower main-shape state before integrating the prototype. | A detected mid attack should retain a visible response even when the shared ripple is suppressed by its cooldown. See [production shader](../shaders/ribbon.frag#L37), [candidate centerline](../tools/macro-motion-prototype/ribbon-prototype.frag#L36) and [candidate fallback](../tools/macro-motion-prototype/PrototypeRibbon.qml#L134). |
| All shared attack ripples use the same spatial wavelength, speed and decay. Band content changes the origin, not the ripple's character. | Consider bounded differences in ripple width and damping from the latched spectral balance of the attack. | Preserve recognizable bass/mid/high roles without adding random vibration. See [ripple formula](../shaders/ribbon.frag#L39). This is a refinement proposal, not an implemented improvement. |
| The fine high-band displacement is a sine pattern moved by the shared phase, scaled by sustained treble. Its maximum is 0.16 logical pixels at 40 px height. Actual treble accents instead affect localized brightness. | Prefer the existing attack-driven highlights, and evaluate any extra fine movement locally around real accents. | Increasing the continuous sine alone would make more motion visible without improving its musical specificity. See [texture](../shaders/ribbon.frag#L58) and [highlights](../shaders/ribbon.frag#L75). |

The main regression is confined to the prototype. Bass still changes its thickness, treble still produces highlights, and mid attacks can still trigger the shared ripple. It is the separate, fast mid-accent deformation that disappeared. The prototype also omits the old bass-accent centerline amplitude term while retaining the thickness response. That is less severe because a visible bass response remains.

Timing merits a later check: there is one shared ripple slot, with a minimum 160 ms trigger interval and 190 ms exponential decay constant. A new trigger replaces the previous age, strength and origin. There is no overlap or blend between events. A formula-only example with equal-strength attacks 170 ms apart at opposite band origins can change displacement by about 1.21 logical pixels in a 40 px view. This is a possible discontinuity, not a measured glitch in a song or a newly reproduced rendering failure.

The current level/attack separation is useful. Short band envelopes rise with a 12 ms time constant; their peak/release constants are 65/85 ms for bass, 45/65 ms for mids, and 35/50 ms for treble. These are filter constants, not total audio-to-screen latency. The analyzer deliberately rejects repeated accents from steady tones, small vibrato and stationary texture. It does not reproduce the physical vibration frequency of strings or voices.

The existing ripple is restrained at panel size. The rendered strong-attack fixture displaces the ribbon by about 1.10 logical pixels at 200 × 40. The analytical absolute bound is 1.28 pixels at that height. The shared ripple pattern has a spatial period of about 26.18 pixels at 200 px width and a nominal temporal carrier of 5.35 Hz under the travelling-wave expression, with a short decaying envelope. These are chosen visual parameters, not estimated musical pitch or tempo. They remain unchanged in the main-shape prototype.

## Verification

Evidence is in `build/evidence/vibration-review/`:

- Reran `test-musical-response`: exit 0. The suite covers band-selective masked accents, equal-level note changes, close notes, repeated accents and restraint on steady input. Synthetic 48 kHz accent threshold crossings were 16 ms in the listed fixtures, excluding GUI presentation and physical playback latency.
- Reran the production `musicalAccents`, `rippleAtPanelSize` and `attackHasNoCentralCrease` view cases: 13 passed, 0 failed, including setup/cleanup. Both renderers retain separate band responses in the production sources. The strong-ripple peaks were 1.100–1.103 logical pixels at the three origins.
- Built a separate Qt image diagnostic against the actual compiled baseline and candidate shaders. The fixed input was energy 0.65, bass 0.4, mids 0.5, treble 0.3 and phase 0.65. It changed exactly one accent from 0 to 0.8 while suppressing shared onset ripples and dynamic color. The candidate's four shape values stayed fixed to isolate fast response.
- On the 200 × 40 ShaderEffect render, changing the mid accent altered 4567 RGB channels by more than 2 code values in the baseline, versus zero in the candidate. The candidate maximum byte difference was also zero. Canvas changed 1726 channels in the baseline and zero in the candidate. Bass and high accents still changed the candidate image in both renderers.

`accents.log` contains the image differences. `mid-accent-comparison.png` shows baseline above and candidate below, before on the left and after the isolated mid accent on the right. It is enlarged 2× using nearest-neighbor sampling for inspection. `Accent.qml`, `accents.cpp`, `shaders.qrc`, individual PNGs, `musical-response.log`, `view.log` and `formula-metrics.json` preserve the diagnostic.

Verdict: **Approve the current production fine-motion behavior as a restrained baseline; Block integration of the unchanged main-shape prototype until the missing mid-accent response is corrected.** More band-specific ripple character is a possible next refinement. Increasing vibration everywhere is not supported by this review. Long listening and physical Bluetooth synchronization remain unverified here.
