# Luma Ribbon

Panel plasmoid for KDE Plasma 6. One continuous, transparent ribbon driven by audio. Aurora, Ember, Ice, Grove, Iris, Coral and Hue palettes. No player, artwork, or additional effects.

The widget icon is `org.kde.plasma.lumaribbon`, installed from `icons/hicolor/scalable/apps/` into the hicolor theme. Package and native metadata use the same name; CMake installation and the manifest-based uninstaller include the SVG.

- **Monitor**: monitor ports of the default Audio/Sink node. Never a microphone source.
- **Audio block**: 512 stereo float frames with sample rate and capture generation.
- **Analyzer**: Qt-independent module accepting samples and returning normalized energy, bands, attacks and slow gated timbre descriptors for color.
- **Audio snapshot**: the latest analysis result. No queue of frames awaiting the GUI.
- **Ribbon motion**: worker-owned `RibbonMotion` turns band balance and phrase changes into a shared arch, counter-bend, bias and filament opening. Slower lift and lean follow RMS and timbre trends before display normalization. Views never run their own springs.
- **Ribbon view**: panel, popup or draft settings preview, sharing the shader and audio module. Per-instance curvature, fullness and bloom scale rendering only, leaving shared motion and analysis unchanged. Audio-reactive colors stay within the selected palette and can be disabled for the original fixed gradient.
- **Settings preview**: uses the applet's existing AudioState and current logical panel size. Draft settings do not change the saved applet configuration or its renderer diagnostics. Apply persists them through Plasma's KConfigPropertyMap.
- **Hue palette**: saved index 6 adds a complete simultaneous spectrum. Six cached OKLCH anchors and a repeated endpoint keep work bounded. Audio warps the color distribution slightly without cropping any hue or introducing a clock-driven cycle. The shader and Canvas share those anchors.
- **Hue shift**: a per-instance offset in degrees rotates only the Hue spectrum in OKLCH. Its control is visible only for palette index 6. The saved offset survives palette switches but is ignored by the other six presets, including their dynamic colors. Conversion and gamut mapping remain cached outside audio updates.

Priorities: passive monitor capture, bounded memory, stability, and visual quality at the default 120 × 40 panel size. Automated tests exercise the interfaces used by the application. Unperformed hardware checks must be identified explicitly. The interface and documentation are in English.
