# Official references

Checked on 2026-09-05 against the installed headers and these upstream documents. The implementation deliberately uses the Plasma 6 API; some older KDE C++ widget examples still describe Plasma 5's `nativeInterface`.

| Subject | Source and decision |
| --- | --- |
| Plasma 6 applet properties | [Porting Plasmoids to KF6](https://develop.kde.org/docs/plasma/widget/porting_kf6/): `PlasmoidItem` root and `Plasmoid` referring to the C++ Applet |
| Configuration | [Plasma widget configuration](https://develop.kde.org/docs/plasma/widget/configuration/): KConfig XML and `cfg_` properties |
| Configuration engine lifetime | [Plasma 6.7 ConfigView source](https://github.com/KDE/libplasma/blob/Plasma/6.7/src/plasmaquick/configview.cpp): separate QML engine per settings window, deletion after hiding |
| Testing a widget | [Plasma testing](https://develop.kde.org/docs/plasma/widget/testing/): separate `plasmawindowed` process |
| Native capture | [PipeWire audio-capture example](https://docs.pipewire.org/audio-capture_8c-example.html): input stream, `SPA_PARAM_Format`, mapped buffers and capture-sink option |
| Stream teardown | PipeWire upstream [1.6.8 stream source](https://github.com/PipeWire/pipewire/blob/1.6.8/src/pipewire/stream.c) and [0.3.65 stream source](https://github.com/PipeWire/pipewire/blob/0.3.65/src/pipewire/stream.c): disconnect before listener removal, including releases without synchronized hook removal |
| Routing properties | [PipeWire properties](https://docs.pipewire.org/page_man_pipewire-props_7.html): serial target, no fallback, no automatic reconnect/move, passive links |
| Capture key | [PipeWire key names](https://docs.pipewire.org/group__pw__keys.html): `PW_KEY_STREAM_CAPTURE_SINK` |
| Buffer layout | [SPA chunk](https://docs.pipewire.org/structspa__chunk.html): offset modulo maximum size, valid size and stride |
| Isolated policy tests | [WirePlumber features](https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/features.html): policy separate from hardware monitors; installed `policy` profile |
| Shader interface | [Qt ShaderEffect](https://doc.qt.io/qt-6/qml-qtquick-shadereffect.html): qsb resources, uniforms and premultiplied output |
| Dynamic palette colors | [Qt color value type](https://doc.qt.io/qt-6/qml-color.html): typed color channels and `Qt.rgba`; opaque interpolated palette inputs retain the shader's existing alpha behavior |
| Perceptual palette interpolation | [W3C CSS Color 4](https://www.w3.org/TR/css-color-4/#interpolation-space): OKLCH for retaining chroma between hues; the project implements the color math in QML JavaScript, not CSS |
| Color conversion matrices | Bjorn Ottosson's public-domain [Oklab reference](https://bottosson.github.io/posts/oklab/), using the 2021-01-25 matrices and an sRGB transfer function |
| QML JavaScript resource | [Qt JavaScript resources](https://doc.qt.io/qt-6/qtqml-javascript-resources.html): the stateless palette helper is imported as a `.pragma library` script |
| Shader build | [Qt Shader Tools build integration](https://doc.qt.io/qt-6/qtshadertools-build.html): `qt_add_shaders` |
| Software backend | [Qt GraphicsInfo](https://doc.qt.io/qt-6/qml-qtquick-graphicsinfo.html): renderer selection |
| Frame timer | [QML Timer](https://doc.qt.io/qt-6/qml-qtqml-timer.html): animation-clock synchronization and timing limits |
| FFTW license | [FFTW license and copyright](https://www.fftw.org/fftw3_doc/License-and-Copyright.html): GPL-2.0-or-later dependency |
| Spectral onset detection | Simon Dixon, [Onset Detection Revisited](https://www.dafx.de/paper-archive/2006/papers/p_133.pdf), DAFx 2006: positive magnitude changes can expose note entries hidden in total energy. Luma uses an RMS-style aggregation within each of its three bands |
| Vibrato restraint and dense notes | Sebastian Böck and Gerhard Widmer, [Maximum Filter Vibrato Suppression for Onset Detection](https://www.dafx.de/paper-archive/2013/papers/09.dafx2013_submission_12.pdf), DAFx 2013: their maximum filter operates on a musical frequency scale, and peak selection uses a local mean. This motivated Luma's softer low-frequency neighbor weighting and symmetric adaptive mean. Luma does not implement their full SuperFlux system |
| Arch packages | [libplasma](https://archlinux.org/packages/extra/x86_64/libplasma/) and [qt6-shadertools](https://archlinux.org/packages/extra/x86_64/qt6-shadertools/) |

The source also follows the requested Karpathy, Codebase Design, Apple Design and Emil Design Engineering guidance, with pstack's Boundary Discipline and Prove It Works. The architecture review concentrated on real queue/cadence and lifecycle defects rather than speculative abstractions.
