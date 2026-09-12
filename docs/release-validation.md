# Release validation

Run against the commit being released, in a fresh process that loads its native plugin and QML. Record the commit, distribution, Plasma/Qt/PipeWire/WirePlumber versions, GPU/driver, output device and display scales with each result. Older reports remain historical evidence.

## Automated checks

The CI workflow builds the analysis-only target with ASan/UBSan on Ubuntu 24.04 and the complete plugin on Debian 13. Desktop tests run in their own Xvfb display and D-Bus session. OpenGL uses Mesa's CPU renderer to exercise shaders without a physical GPU; the separate Qt software-renderer run uses 125% scaling. This is not physical GPU, Wayland or mixed-monitor validation.

The Plasma test clicks the host's actual Apply and Cancel/Discard buttons, reopens the dialog, verifies saved settings and exercises Space/Return/Enter on the compact representation. The earlier direct configuration-map assertions remain useful for checking individual visual bindings. Capture tests require a useful connection error, and audio-state tests cover its recovery, diagnostic updates, queue-loss counters and final-owner removal.

The private PipeWire integration script creates only null devices, restarts its own server/policy manager, checks the lack of microphone fallback and verifies stream cleanup. Staged install/uninstall also runs in CI. A passing job does not install the widget into a user's desktop.

## Resource measurements

The optional Python tool measures a separate capture-only process and prints JSON. It records no PCM and does not control playback or routing:

```sh
python3 tools/measure_audio_resources.py build/luma-audio-probe --unavailable --seconds 30
python3 tools/measure_audio_resources.py build/luma-audio-probe --seconds 60
```

`--unavailable` uses an empty private runtime directory. Without it, measure once with a silent default output and once with ordinary playback. Keep duration, build type and machine conditions equal for comparisons. The report contains CPU time, CPU percentage of one core, voluntary/involuntary context switches, peak RSS and the probe's status/counters. Context switches are a scheduling proxy, not a hardware wake-up or battery-power measurement. These measurements exclude the widget's GUI and GPU.

The worker waits until its next 100 ms routing poll only when capture reports an error, has no negotiated format and the audio envelope is dark. An existing stream, including a suspended or zero-filled stream, retains its 10 ms cadence. This bounds the change to unavailable-output handling and preserves the normal first-note path. Do not claim a general silence/battery improvement from the unavailable-server benchmark.

## Manual checks before declaring broader compatibility

These checks are **pending** until an actual result is recorded. Use a test desktop or an agreed maintenance window for actions that interrupt hardware or the session.

| Scenario | Procedure | Expected result / evidence |
| --- | --- | --- |
| Physical output changes | Play ordinary audio; switch USB, HDMI and Bluetooth outputs, then unplug/reconnect each | Ribbon follows the current default output, fades on loss and resumes automatically. Record device, recovery time and diagnostics; no microphone stream |
| Whole-system suspend | Suspend and resume during playback and during silence | No stale flash, frozen ribbon or leaked monitor; playback response returns. Record diagnostics before and after |
| Mixed displays | Move the panel between displays at different scales and refresh rates; resize and rotate it | Stable reserved length, unclipped ribbon and usable settings preview. Record physical captures and display configuration |
| Panel lifecycle | Exercise auto-hide, edit mode, desktop placement, popup, keyboard focus and two widget instances | Hidden views stop rendering; a single shared monitor remains until the final instance is removed |
| Long operation | Run for at least eight hours with playback/silence cycles | No continuing memory/thread growth, stuck errors or capture leaks. Compare diagnostics and process resource samples at start/end |
| Physical timing | Observe attacks with wired and Bluetooth playback | Record perceived/measured offset separately for each device. Capture timestamps alone do not establish speaker synchronization |
| Themes and drivers | Check dark/light/transparent panels and available GPU backends | Readable colors, transparent edges, visible keyboard focus and working software fallback |

Record **pass**, **fail** or **not run** per row, with evidence and a linked issue for failures. A null-sink test or a synthetic screenshot does not close a hardware row.
