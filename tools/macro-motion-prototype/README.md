# Main-shape prototype

Disposable native Qt study of one question: can the ribbon change its broad silhouette with audio, instead of repeating a travelling wave? The left column preserves the earlier travelling-wave shader; the right column uses the original candidate. The applet now has a worker-owned implementation of this design, documented in [shared motion integration](../../docs/shared-motion.md). Every view receives the same analysis snapshot, phase, colors and attack envelopes.

Run from the repository root. The script builds a separate executable without installing anything:

```sh
bash tools/macro-motion-prototype/run.sh
```

Default input is the native monitor of the default audio output. Start music in your own player. The tool never starts playback or changes routing.

For a repeatable study without capturing desktop audio:

```sh
bash tools/macro-motion-prototype/run.sh --synthetic
```

This repeats 26 seconds of generated PCM through the actual FFTW analyzer, including bass, mids, highs, changing blends, a quiet passage and four seconds of silence. The preview plays no sound. The recording command below also saves the generated PCM for an optional video soundtrack.

The default **Isolate main shape** checkbox holds the gradient fixed and suppresses short attack effects in both columns. Clear it to include the existing dynamic colors and ripples. Reduced motion, simple rendering and all six palettes can be compared in the same window. The displayed shape values belong to the candidate; the historical baseline shader does not use them.

Additional commands after the first build, run from the repository root:

```sh
build-macro-motion-prototype/tools/macro-motion-prototype/luma-motion-prototype --full
build-macro-motion-prototype/tools/macro-motion-prototype/luma-motion-prototype --synthetic --at 6 --capture /tmp/luma-shape.png
QT_SCALE_FACTOR=1.25 build-macro-motion-prototype/tools/macro-motion-prototype/luma-motion-prototype --synthetic --full
QT_QUICK_BACKEND=software build-macro-motion-prototype/tools/macro-motion-prototype/luma-motion-prototype --synthetic

# One fixed 26-second sequence, with at most one pending capture.
build-macro-motion-prototype/tools/macro-motion-prototype/luma-motion-prototype --synthetic --record build/motion-recording
ffmpeg -framerate 30 -i build/motion-recording/frames/%04d.png \
    -f f32le -ar 48000 -ac 2 -i build/motion-recording/synthetic.f32 \
    -c:v libx264 -pix_fmt yuv420p -crf 18 -c:a aac -b:a 160k -shortest build/motion-recording.mp4
```

`--full`, `--reduced`, `--fallback` and `--palette 0|1|2|3|4|5` also apply to captures and recordings. The palette order is Aurora, Ember, Ice, Grove, Iris, Coral. `--trace FILE` writes displayed features and shape values, including with live input. `--capture FILE` closes the window after five seconds, or after rendering the frozen `--at` frame. Close the interactive window to release its capture and analysis resources.

The build option `LUMA_BUILD_MOTION_PROTOTYPE` defaults to `OFF`. This directory has no install rule. It deliberately contains a separate view adapter and shader to keep the experiment out of the applet. The prototype computes motion once in its GUI controller; the production implementation now puts that state in the shared audio worker and publishes it in `AudioSnapshot`, with separate lifecycle and rendering checks. Do not install this adapter as the widget.

See the [review and evidence](../../docs/macro-motion-prototype.md). The prototype adds no dependencies to the normal applet; recording a video additionally requires FFmpeg.

The [independent mid-accent correction](../../docs/mid-accent-fix.md) restores fast mid response in both renderers. With `BUILD_TESTING=ON`, this optional prototype also provides the `test-prototype-accents` target and `prototype-accents` CTest case. The correction report lists commands for its separate test build, normal scale, 125% and software rendering.
