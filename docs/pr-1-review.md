# PR 1 review

Date: 2026-09-06. [Pull request](https://github.com/Lucenx9/lumaribbon/pull/1).
Comparison: base `ff4e63e429b37e8aa73ce7359f3fea4749e0d007` to submitted head `1177c1232a44bb90af39c4b98b1e29443ec0f6ae`, followed by the regression-test additions described below.

## Standards

No actionable documented-standard violations or code smells were found. The changes preserve Qt-independent analysis and motion, bounded worker retry, shared snapshot ownership and palette ownership. The follow-up test changes also passed an independent review.

Two coverage suggestions were implemented. A separate `engine-unexpected-recovery` CTest case injects a non-`std::exception` through the test-only FFTW shim and checks error publication, automatic retry and final-owner removal within 500 ms during backoff. Motion tests now supply NaN, positive infinity and negative infinity to each consumed feature independently, checking finite output before comparing the held shape.

## Spec

No missing requirements, unintended scope changes or confirmed implementation defects were found against the PR description. The worker catch-all, capture discontinuity reset, finite-feature guard, palette extraction, tooltip and CMake changes match their stated purposes.

The review comment proposing later software-renderer detection was investigated and not applied. Qt permits renderer API queries once an item has an associated window; it does not require scene-graph initialization. `QQuickGraphicsInfo::setWindow()` calls `updateInfo()` synchronously. See the [Qt renderer-interface documentation](https://doc.qt.io/qt-6/qsgrendererinterface.html#details) and [Qt 6.11.2 implementation](https://github.com/qt/qtdeclarative/blob/v6.11.2/src/quick/items/qquickgraphicsinfo.cpp#L190).

A native probe using `QQuickWindow::setSceneGraphBackend("software")` with `QT_QUICK_BACKEND` unset reported `software=true`, `fallback=true` and `shaderFailed=false` immediately after `setSource()`, unchanged after rendering. More directly, the unmodified test executable selected software under the minimal platform plugin and skipped all five shader-only cases through the new guard:

```sh
env -u QT_QUICK_BACKEND QT_QPA_PLATFORM=minimal \
  build/test-view attackHasNoCentralCrease rippleAtPanelSize
```

## Verification

Environment: CachyOS, Qt 6.11.2, Plasma 6.7.4, PipeWire 1.6.8, FFTW 3.3.11 and GCC 16.2.1. A fresh RelWithDebInfo build used CMake's Unix Makefiles generator and produced the native plugin and compiled shaders.

| Check | Result |
| --- | --- |
| Complete CTest suite | 10/10 passed |
| View, motion-view and isolated Plasma tests at 125% | 3/3 passed |
| Software view suite, selected through the public Qt API without `QT_QUICK_BACKEND` | 60 passed, five intentional shader-only skips |
| Automatic software selection using the minimal platform plugin | Five expected ripple-test skips; no failures |
| ASan, UBSan and leak detection | Musical response, motion, both recovery cases and audio-state passed, 5/5 |
| Private PipeWire/WirePlumber integration | All eight checkpoints passed, including output switching, metadata loss/recovery, silence, no microphone fallback, server restart and final-owner cleanup |
| Regression sensitivity | Substituting each corresponding pre-PR implementation causes the motion finite-output assertion, quieter-device accent assertion and unexpected-exception worker case to fail |
| QML lint | RibbonView clean; main.qml retains the known native `Plasmoid.audio` static-type warning |
| Diff whitespace | Passed |

The software API run used a disposable copy of `test_view.cpp` with only its test entry point changed to select the backend. The regression sensitivity runs linked the current tests against the relevant pre-PR source, leaving the working tree unchanged. The old worker terminated on the injected exception; core dumps were disabled for that test process.

Evidence is local under `build/evidence/pr-1-review/`; private-server probe logs are under `build-pr-1-review/evidence/`. These generated files are excluded from Git. This pass did not install the update or assess live playing music on the user's panel. Plasma tests used a separate test containment and staged plugin. Desktop services, playback and audio routing were not restarted or changed. Other Qt versions and graphics-driver failures were not exercised.

Standards: 0 findings, highest severity none. Spec: 0 findings, highest severity none.
