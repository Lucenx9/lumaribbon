# Verification record

The [reliability review](reliability-review.md) records the 2026-09-12 CI, headless-build, diagnostics, keyboard and actual configuration-dialog checks on Debian 13. The [release procedure](release-validation.md) separates automated checks from pending hardware validation.

The [panel-depth pass](panel-depth.md) records the correction for flat mixed-band shapes, wider filaments and soft panel edges. The complete suite passes 10/10, with 50 motion-view entries including the new PCM-to-render regressions.

The [PR 1 review](pr-1-review.md) records the 2026-09-06 hardening checks and added exception/motion regressions. Its complete suite passes 10/10; the records below describe earlier passes.

Date: 2026-09-05. Environment: CachyOS, Wayland, Plasma 6.7.4, Qt 6.11.2, PipeWire 1.6.8, FFTW 3.3.11, GCC 16.2.1, CMake 4.4.3.

## Executed

| Check | Result |
| --- | --- |
| CMake configure/build | Passed, including native applet plugin and qsb shader generation |
| Signal analysis | Passed: silence, 100/1000/3200 Hz at 8/44.1/48/96/192 kHz, RMS amplitude, band isolation, impulses, finite values, DC removal, anti-phase stereo, noise gate and silence decay |
| Queue/worker cadence | Passed: 28 combinations of rate and quantum, bounded overflow, capture generation change and recovery |
| Partial block after pause | Passed: 256 tone samples, five seconds without input, then silence at the same format/generation produces no light or false attack |
| Concurrent ring | Passed: 100,000 blocks, monotonic sequence, no torn blocks, all accepted blocks consumed and drops accounted for |
| Initialization recovery | Passed with injected FFTW plan failures: error status, automatic recovery after backoff, and final-owner removal within 500 ms while retrying |
| Qt Quick view | Passed: nonempty transparent rendering, 30/60 limits, hidden-view/window suspension, software fallback, vertical resize, no unchanged-silence repaints |
| Phase wrap continuity | Passed pixel comparisons at `200*pi` and zero for shader and Canvas; also passed with the software backend |
| Observed update rate | 32 updates over 1.1 seconds at the 30 FPS limit; 64 over 1.1 seconds at the 60 FPS limit, on this machine |
| Fractional scaling | Qt Quick view tests passed with `QT_SCALE_FACTOR=1.25`; screenshot inspected at 1075 × 775 physical pixels for an 860 × 620 logical window |
| Software backend | Qt Quick view tests passed with `QT_QUICK_BACKEND=software`; fallback screenshot inspected |
| Actual Plasma applet | Passed in a separate test Corona using the installed desktop shell package: native loading, compact panel representation, click-to-expand, popup creation, configuration form, horizontal-to-vertical change, two applets and removal |
| User's actual panel | Temporarily added two instances to the existing 46-pixel bottom panel: exactly one capture monitor; first removal preserved capture, final removal released it. Original widget IDs/order and the Plasma process were preserved. Playback was silent during this lifecycle check |
| Plasma Windowed | Staged package loaded in `plasmawindowed` without applet QML errors |
| Real audio | Captured the user's playing default AirPods output at stereo 48 kHz. Features stayed finite and normalized, zero queue drops observed, engine released on exit. Playback was not controlled or modified |
| Default output changes | Passed in a private PipeWire/WirePlumber instance: null sink A to null sink B |
| Policy metadata replacement | Passed: private WirePlumber-only restart, bounded missing-metadata error, no indefinitely retained stream, and automatic capture recovery |
| No microphone fallback | Passed in that private server after all Audio/Sink nodes were removed while an Audio/Source node remained; capture stream was absent |
| Device disappearance | Passed in that private server, followed by a meaningful error status |
| PipeWire restart | Passed in that private server: disconnect reported, new server and sink C connected automatically, audio resumed |
| Last owner cleanup | Private server contained no Luma capture stream after the probe's final shared owner was released |
| QML lint | RibbonView and preview QML passed without warnings |
| ASan / UBSan / LeakSanitizer | Passed on final code: signal/queue/cadence tests, injected initialization failure/retry/removal, and the full private PipeWire integration suite with active synthetic audio and leak detection enabled. The capture-only probe also passed against the desktop server |
| Install / uninstall / reinstall | Passed in a staged `/usr` tree using the real CMake install manifest and uninstall target |
| System installation | Installed plugin, package and documentation under `/usr`; `kpackagetool6` discovers the applet and the native library has no missing dependencies |

The original standard suite passed 4/4 tests. The current suite, `ctest --test-dir build --output-on-failure`, passes **7/7**, including musical-response, color-response and audio-state regression suites. The private integration suite is `bash tests/pipewire_integration.sh build`. Its original logs are in `build/evidence/pipewire-integration.log` and `pipewire-events.log`; the real-panel lifecycle result is in `real-panel.log`. The newest timing/response results and log locations are recorded below. The screenshots are actual Qt Quick captures, not generated artwork.

An independent architecture review found and prompted fixes for small-quantum queue starvation, a fixed missing-input timeout, errors hidden during retry, and a partial block retained across suspension. Each timing defect received a regression test.

The requested GLM-5.3 and Gemini 3.8 Flash High reviews completed. See [review findings and corrections](reviews.md) for provider/harness details and the final disposition of each finding.

The subsequent [desktop review](desktop-review.md) exercised every visual setting on the user's actual widget with Spotify playing. It added tests for all three palettes in both renderers, intensity endpoints, sensitivity forwarding, phase-independent reduced motion, renderer switching and Canvas update cadence. All 11 view cases passed on the final code at normal scale, 125%, and with the software backend. The complete CTest suite passed 4/4 after these changes. The Plasma configuration test now reproduces the host's initial-property map, including the page title and generated defaults. The staged test package is refreshed on every build to avoid testing stale QML. These visual/QML changes did not alter the previously instrumented audio backend.

To repeat the instrumented backend checks:

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DLUMA_BUILD_PREVIEW=OFF \
    -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build build-asan --parallel --target test-analysis test-musical-response test-color-response test-engine-recovery test-audio-state luma-audio-probe luma-test-audio
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan -R '^(analysis|musical-response|color-response|engine-recovery|audio-state)$' --output-on-failure
ASAN_OPTIONS=detect_leaks=1 bash tests/pipewire_integration.sh build-asan
```

The instrumented integration log is `build-asan/evidence/pipewire-integration.log`; all eight checkpoints passed with zero queue drops. Graphical tests were run separately in the regular build.

The earlier [visual refinement](visual-refinement.md) passed the complete 4/4 CTest suite, 18 view checks at normal scale and 125%, and 17 software-renderer checks with one intentional shader-only skip. It adds palette/theme contrast and alpha-preservation checks plus a regression for an angular attack ripple. The updated installed package was inspected in a fresh Plasma Windowed process with real Spotify audio on the default HDMI output, stereo 48 kHz, with zero observed queue drops. No backend code changed during that visual pass.

The first [musical response refinement](musical-response.md) added independent spectral accents, kept sustained-level normalization, and advanced ripple timing in the analyzer. Its complete suite passed 5/5; the view suite passed 24 checks at normal scale and 125%, and 23 with one intentional skip in software. Instrumented signal, musical-response and recovery checks passed. The private PipeWire suite passed all eight checkpoints again. A same-input comparison ran both analyzers on 30 seconds of real Spotify output on AirPods, stereo 48 kHz, with zero errors or dropped blocks. Captured synthetic frame sequences and a 14-second video show those musical changes without recording the user's audio.

The [close-note and timing refinement](note-timing.md) passes 20/20 semitone fixtures and 304/304 fast-accent rises, compared with 7/20 and 103/304 for the preceding analyzer compiled against identical fixtures. It also checks full bass phrases and vibrato restraint. The full suite passes 5/5; view checks remain 24 at normal scale and 125%, and 23 plus one skip in software. All three instrumented backend suites and the eight-checkpoint private PipeWire integration suite pass. Plan-replacement failure is now injected and recovered. A fresh real-audio comparison captured the default HDMI monitor, stereo 48 kHz, with zero errors or drops. Detailed resource costs and unmeasured listening/latency limits are in the report.

## Still requiring manual/hardware validation

- Prolonged use inside the user's actual panel, including panel auto-hide, multiple screens, panel resizing, edit mode and interaction with different Plasma themes.
- Physical hot-unplug and reconnection of USB, HDMI and Bluetooth outputs while audio is playing. Device switching and disappearance were automated on private null sinks; the real AirPods check was capture-only.
- Changing the desktop's actual display scale and moving the panel between displays with different scale factors. The tests used a Qt process at 125% scaling.
- Vulkan and other GPU drivers. The available graphical environment rendered the shader successfully, but this is not a driver compatibility matrix.
- Overnight operation, suspend/resume of the whole computer, session-manager policy customizations and unusual multichannel hardware.

The desktop session and its PipeWire/WirePlumber services were not restarted. Tests that restart services use their own runtime directory and D-Bus session.

## Audio-reactive color pass

The [color response review](dynamic-color.md) documents the new optional palette movement, its implementation and actual rendered comparisons. The new color regression suite checks 15 rate/band combinations plus smoothing, silence, suspension, noise rejection, volume invariance and reset. All four instrumented backend suites passed with ASan, UBSan and leak detection enabled. The real-output probe captured 20 seconds on HDMI at stereo 48 kHz with zero errors or drops. Color calculations do not change capture, routing or queue behavior; the private device-switch/restart checks above were performed during the preceding note/timing pass.

The full CTest suite passed 6/6. View tests passed 30/30 at normal scale and 125%, and 29 with one intentional shader-only skip in software. A small-mix visibility failure for Ice/Canvas on a light background at 125% was reproduced, corrected and retested. QML lint produced no warnings. The new configuration checkbox was clicked in a rendered native form, and its setting reached both compact and expanded representations. The installed plugin passed the native Plasma test in a fresh process, including popup and removal. Final configuration and live-output preview screenshots were inspected. Plasma PID 107442 remained running and Spotify remained in Playing state.

## Perceptual color pass

The [OKLCH refinement](perceptual-color.md) changes only palette interpolation, tests and documentation. The six palette/theme cases now verify perceived lightness, retained chroma, continuous lookup steps, a bounded 65-entry table, no table rebuild during audio updates and exact fixed-mode colors. Five of the preceding RGB cases failed the new lightness check; the updated six cases pass. The full CTest suite passed 6/6. View tests passed 36 at normal scale and 125%, and 35 with one intentional shader-only skip in software. QML lint passed without warnings.

The installed package passed the native Plasma test in a fresh process, including settings, popup and removal. Synthetic before/after frames, a 14-second comparison and a live-output preview were inspected. SHA-256 checks confirm every `src/` file and the fragment shader are unchanged from before this pass; earlier sanitizer and routing results remain historical and were not rerun for this QML-only change. Installation includes the new JavaScript resource and was compared against the source package. At completion, Spotify was Playing and one Luma monitor remained for the desktop widget. No desktop process or audio service was restarted by this work. The running desktop had been started before this pass and may retain the earlier QML until a normal reload or login.

## Wake-up, freshness and attack-origin pass

The [timing and response report](timing-response.md) records the shared silence wake-up, expiration of stale queued audio and spectral ripple origins. CTest passed 7/7. View checks passed 41 at normal scale and 125%, and 36 plus five intentional shader-only skips with the software backend. QML lint passed without warnings. The new audio-state test uses the actual engine and state with a deterministic capture adapter, including a blocked GUI, coalescing, shared ownership and removal with a pending event.

The five instrumented backend suites passed with ASan, UBSan and leak detection. The private PipeWire integration passed all eight checkpoints on freshly rebuilt instrumented binaries. A 20-second real capture on the default AirPods output at stereo 48 kHz had no errors, overflow or expired blocks. The synthetic before/after comparison and live Qt Quick preview were inspected. The experimental FrameAnimation scheduler was rejected after measuring worse frame regularity; the display timer remains unchanged.

Final evidence is in `build/evidence/timing-response/`: `ctest-final.log`, `view-125.log`, `view-software.log`, `asan-final.log`, `private-pipewire-final.log`, `private-probe-final.log`, `live-audio.log`, and the `pacing/` probes. The repeatable sanitizer command above was repaired and its accidentally duplicated documentation section removed; the documented shell block passes `bash -n`.

The updated package was installed under `/usr` and compared against the build/source files. The fresh installed-plugin Plasma test passed all three QtTest entries, including popup, settings, two instances and removal. Desktop Plasma PID 264298 remained running with the preceding native library mapped; the installed C++ changes will load with a fresh Plasma process at the next normal login. Spotify remained Playing and exactly one desktop Luma capture node remained. No desktop process or desktop audio service was restarted.

## Color depth pass

The [wider gradient refinement](color-depth.md) changes the two endpoint bindings in the applet and the isolated main-shape prototype. The normal project and optional prototype build successfully. The focused CTest selection passes 2/2, comprising 41 view entries and three native Plasma integration entries. The color, perceptual interpolation and theme checks pass 20/20 at 125% scaling and 20/20 with the software backend. Both modified QML files pass lint without warnings.

The original small-mix visibility threshold is retained. Light-theme Ice with Canvas measures 4.82 channel levels per visible pixel at normal scale and 4.72 at 125%, above the threshold of four. These are rendered regression metrics, not perceptual quality scores. Evidence is in `build/evidence/color-depth/`. Audio sources and the production shader are unchanged; backend, recovery and sanitizer tests were not rerun for this color-only change.

The updated package and plugin were installed under `/usr` and compared against source/build files. The installed applet passes all three native Plasma test entries in a fresh process. The final 14-second comparison uses generated PCM; representative frames include the transition, a mixed passage and the silence tail. A live preview and short probe found the default Sunshine monitor connected at stereo 48 kHz with no audio, errors, drops or expired blocks. Playing-song visual assessment remains unperformed in this pass. Plasma PID 264298 was unchanged. The isolated main-shape prototype received matching color bindings and a synthetic visual check but remains excluded from installation.

## Grove and Iris palette pass

The [Grove and Iris addition](grove-iris.md) appends palette indices 3 and 4 and shares the palette catalog between the applet, configuration and preview tools. Both normal and optional prototype builds pass. The focused CTest selection passes 2/2, with 57 view entries and three Plasma entries. All five configuration labels and their propagation to panel/popup are checked. The 42-entry color/perceptual/theme/settings selection passes at 125% and with software rendering. Existing palette values, indices and regression thresholds are preserved.

The updated five-palette generated-PCM preview and the new dark/white palette comparison were captured and inspected. The native prototype also captured both `--palette 3` and `--palette 4` with dynamic colors enabled. Rendering QML lint is clean; the configuration's pre-existing native `Plasmoid.audio` static-type warning is documented in the palette report. Evidence is in `build/evidence/grove-iris/`. Capture, analysis and the production shader have unchanged hashes. This pass does not repeat backend tests or claim a live playing-song assessment.

The source package and built plugin match the installation under `/usr`. All three installed-app Plasma test entries pass in a fresh process, including the five palette choices. Desktop Plasma PID 264298 is unchanged; the existing panel can retain earlier QML or configuration schema until its next normal reload/login. No desktop or audio service restart was performed.

## Coral palette pass

The [Coral palette](coral.md) appends saved index 5 with pink-to-orange colors for dark and light backgrounds. The normal project and optional prototype compile. The focused CTest selection passes 2/2, comprising 65 view entries and three Plasma entries. All six palette names and their panel/popup propagation are checked. The existing five palettes retain their exact names, indices and ten color triplets. The color/perceptual/theme/settings selection passes 50 entries at 125% scaling, with the existing thresholds retained.

Evidence is in `build/evidence/coral/`. Rendering QML lint is clean. Capture, analysis and the production shader have unchanged hashes. Backend recovery and sanitizer checks are not repeated for this palette-only addition.

The same 50-entry selection also passes with the software backend. Native captures of Coral and Ember on dark/white backgrounds, the six-palette generated-PCM preview and the prototype's `--palette 5 --full` selection were inspected. No new live-song assessment is claimed.

The package and native plugin match the installation under `/usr`. The fresh installed-app test passes all three Plasma entries, including all six palette choices. Desktop Plasma PID 264298 remains running. No desktop or audio service restart was performed; the active panel can retain its earlier QML/schema until a normal reload or login.

## Ember fire-red pass

The [Ember refinement](ember-fire.md) changes only its dark/light palette triplets. Its base is red-to-vermilion with orange highlights. All other color triplets, names and saved indices are unchanged. The normal project builds and the ten focused Ember view entries pass at normal scale, 125% and with the software backend. Existing regression thresholds are retained; a first light-theme candidate failed the contrast threshold and was deepened before the final pass.

Native before/after captures, the six-palette generated-PCM preview and the isolated prototype's `--palette 1 --full` view were inspected. The prototype capture includes small, expanded and vertical layouts. Evidence is in `build/evidence/ember-fire/`. Audio sources and the production shader have unchanged hashes; backend recovery and sanitizer tests are not repeated, and no new live-song assessment is claimed.

The installation under `/usr` matches source/build files. All three installed-app Plasma integration entries pass in a fresh process. Desktop Plasma PID 264298 is unchanged; no session or audio service was restarted. The active panel may retain its previous QML until a normal reload or login.

## Shared motion integration

The [shared motion pass](shared-motion.md) integrates the approved broad-shape design into the production worker, audio snapshot, ShaderEffect and Canvas. Palette, capture, analyzer and processor hashes are unchanged. The production preview now uses the same motion module. The earlier prototype retains its historical shader baseline and remains excluded from installation.

The normal build and all nine CTest suites pass. The new `motion` suite exercises analyzed tones, sustained stability, changes between bands, decay, the silence gate, resume and scheduling bounds. The expanded audio-state test verifies a late subscriber, shared state across sensitivities and final-owner teardown. Both rendering suites pass at normal scale and 125%, including 65 existing view entries and 18 motion-view entries. Software rendering passes 60 existing view entries with five intentional shader-only skips, plus all 18 motion-view entries. Motion, audio-state and engine-recovery also pass under AddressSanitizer and UndefinedBehaviorSanitizer with leak detection.

Native before/after views were inspected at panel, expanded and vertical sizes, on dark/light backgrounds, with reduced motion and software rendering. A complete generated-PCM sequence records 780 frames over 26 seconds with finite snapshots. Its sampled broad centerline remains between 0.314 and 0.697 of the view height, with a largest adjacent movement below 0.804 logical pixels at 40 px height. These measurements exclude accents, ripples and glow, and do not measure GPU pacing. The six-second real monitor probe connected at stereo 48 kHz with no errors, dropped or expired blocks and clean shared-engine teardown, but received silence. No playing-song listening assessment is claimed.

Evidence and the generated-signal comparison video are in `build/evidence/shared-motion/`. The installed package/plugin match source/build files. A fresh installed-app Plasma test passes all three entries. The user's Plasma process remains PID 264298 and still maps the earlier plugin image, so this native update takes effect there at the next normal login. Desktop/audio services, routing and playback were not restarted or changed.
