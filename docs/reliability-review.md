# Reliability and release checks

Date: 2026-09-12. Local environment: Debian 13, Plasma libraries 6.3.5 / desktop package 6.3.6, Qt 6.8.2, PipeWire 1.4.2, FFTW 3.3.10 and GCC 14.2. Tests use a separate Xvfb display and D-Bus session. No plugin installation into the live desktop is part of this change.

## Changes

- CI builds without desktop dependencies under ASan/UBSan and builds the complete plugin on Debian 13 with OpenGL and Qt software-renderer checks. The software job also uses 125% scaling. Private PipeWire recovery and staged install/uninstall are included.
- `LUMA_ANALYSIS_ONLY=ON` makes the four analysis/queue/motion suites available without Qt, KDE or PipeWire development packages.
- The compact representation provides keyboard focus, a focus outline and Space/Return/Enter handling. Host configuration tests click Apply, change another draft, Cancel/Discard it, reopen the window and verify the saved value. They wait for focus and the confirmation dialog's opening transition before sending input.
- Settings offer a plain-text diagnostic report with the current error detail and queue-loss counters. The report is selectable and copyable. Showing it does not mark settings as changed. Error details persist during retry backoff and clear on recovery.
- Once an unavailable output has faded to darkness, the worker waits for its existing routing poll instead of waking every 10 ms. Negotiated streams retain their original cadence. The realtime processing callback is unchanged.

## Verification

| Check | Result |
| --- | --- |
| Analysis-only configure/build/test | Four suites passed |
| Native backend | Eight suites passed, including unavailable-output recovery, diagnostic changes and queue-loss reporting |
| ASan and UBSan | The same eight backend suites passed with `detect_leaks=0` and `halt_on_error=1` |
| LeakSanitizer | Not validated locally: its process inspection fails in this environment. The CI analysis job enables leak detection on its runner |
| Rendering | View and motion-view passed with OpenGL through Mesa and with Qt software rendering at 125% |
| Actual Plasma configuration | Passed with OpenGL and software rendering, including Apply/Cancel/Discard, reopening, keyboard activation and diagnostics |
| QML lint | RibbonView, AppearanceControl and Preview passed without warnings |
| Isolated PipeWire | Eight routing/restart/silence/cleanup checkpoints passed in both normal and ASan/UBSan builds; local leak detection disabled |
| Staged install/uninstall | Passed under the build directory |

A captured settings window was inspected with diagnostics expanded at 125%. It retains the host's Apply/Cancel buttons and a scrollable, selectable report. The existing Kirigami warning about a graphical object initially outside the scene also appears on this framework version; the tests check actual interaction and persistence.

## Unavailable-server measurement

The same capture-only probe was built before and after the worker change. Each run used a private empty PipeWire runtime directory and approximately 15 seconds of observation via `tools/measure_audio_resources.py`.

| Metric | Before | After |
| --- | ---: | ---: |
| Wall time | 14.931 s | 15.127 s |
| CPU time | 0.1401 s | 0.0561 s |
| CPU, percentage of one core | 0.938% | 0.371% |
| Voluntary context switches | 1,645 | 291 |
| Peak RSS | 16,708 KiB | 16,416 KiB |

These are local process measurements, not a battery benchmark. Other work was running on the machine. Context switches are a scheduling proxy; the experiment excludes the GUI/GPU and does not measure the cost of a connected silent sink. The lower unavailable-output wake rate follows the 100 ms routing cadence. The tests retain recovery and final-owner teardown bounds.

Hardware hot-plug, actual suspend/resume, Bluetooth presentation timing, mixed physical displays and overnight operation remain **not run** in this pass. Use the [release validation procedure](release-validation.md) to record those results separately. Raw local logs and the diagnostic screenshot are under `build-review/evidence/` and are not committed.
