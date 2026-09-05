# Independent reviews

Date: 2026-09-05. Both requested reviews completed before the final corrections below. The reviewers inspected source rather than approving a screenshot or a design proposal.

| Reviewer | Harness and scope |
| --- | --- |
| GLM-5.3 | OpenCode, explicitly `zai-coding-plan/glm-5.3` through Z.AI Coding Plan. Read the implementation and tests, reran the analysis tests, and checked PipeWire 1.6.8 upstream internals. No OpenRouter provider was used. |
| Gemini 3.8 Flash High | `agy --model gemini-3.8-flash-high --effort high`. Reviewed a complete numbered source bundle in one request. The initial file-reading attempt was blocked by headless command permissions; the completed review used the supplied text without commands or file access. |

Neither reviewer reported a high or critical defect. GLM reported four low-priority findings; Gemini reported two unconfirmed low-priority risks. The phase and teardown concerns overlapped with the other review or subsequent upstream checks.

## Findings and disposition

| Finding | Correction and evidence |
| --- | --- |
| GLM: the native plugin embeds an unnecessary `KPlugin.Id`, producing a warning on load | CMake generates native metadata without `Id`; the installed Plasma package retains its required identifier. The final native Plasma test loads without this warning. |
| GLM: the initial registry/metadata burst can briefly look like a missing default output | Capture allows a neutral wait of at most one second while the default metadata has not arrived. During a metadata replacement it keeps only a previously selected sink that still exists. An explicit missing default, a removed sink or an expired wait closes capture. The private integration test now includes a WirePlumber-only restart and bounded missing-metadata handling. |
| GLM: an analyzer initialization exception leaves the shared engine permanently stopped | The worker publishes an error and rebuilds its pipeline after five seconds. Removal interrupts the backoff. A test-only FFTW wrapper forces initialization failures; `engine-recovery` verifies the error, automatic recovery and final-owner removal within 500 ms. The wrapper is linked only into that test executable. |
| Both: a phase that grows forever eventually loses precision when converted to shader floats | Phase wraps at `200*pi`, the common period of all time coefficients in the shader and Canvas renderer. Pixel comparisons at the wrap boundary pass for both renderers, with a maximum permitted channel difference of 2/255. No overnight run is implied by this test. |
| Gemini: removing the listener before destroying an active stream might race with its realtime callback | PipeWire 1.6.8 already synchronizes hook removal; GLM and the main agent confirmed this in upstream source. The main agent also checked the declared minimum, 0.3.65, which lacks that hook synchronization. Teardown now disconnects the stream before removing its listener. Output switching, disconnect/restart and last-owner release pass on the installed 1.6.8. Runtime tests on 0.3.65 were not performed. |

The earlier architecture review led to regressions for small graph quanta, rate-dependent missing-input timeouts and partial blocks retained across suspension. Its final follow-up found no actionable regression in the phase, retry, teardown, metadata-wait or CMake changes. That follow-up was static inspection only.

## Review limits

The external reviews used a source snapshot preceding these final corrections. Their original notes about pending sanitizer reruns and real-panel checks describe that earlier state; consult [verification](verification.md) for the final results. The raw local review responses remain in `build/reviews/glm-5.3.md` and `build/reviews/gemini-3.8-flash-high.md` and are not build dependencies.

Hardware hot-plug, unusual multichannel routing, mixed-DPI displays and prolonged desktop use remain manual coverage gaps. These reviews do not establish compatibility with every PipeWire, Qt or graphics-driver version.

The later [visual peer review](visual-peer-review.md) uses the specifically requested GLM-5.3-Flash and Gemini 3.8 Flash High, with actual screenshots and the shader/QML at that point. Both approve the first-version design, disagree about panel versus popup quality, and propose refinements. Selected proposals were subsequently implemented and verified in the [visual refinement](visual-refinement.md).
