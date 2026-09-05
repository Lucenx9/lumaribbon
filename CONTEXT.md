# Luma Ribbon

Panel plasmoid for KDE Plasma 6. One continuous, transparent ribbon driven by audio. Aurora, Ember, Ice, Grove, Iris and Coral palettes. No player, artwork, or additional effects.

- **Monitor**: monitor ports of the default Audio/Sink node. Never a microphone source.
- **Audio block**: 512 stereo float frames with sample rate and capture generation.
- **Analyzer**: Qt-independent module accepting samples and returning normalized energy, bands, attacks and slow gated timbre descriptors for color.
- **Audio snapshot**: the latest analysis result. No queue of frames awaiting the GUI.
- **Ribbon motion**: worker-owned `RibbonMotion` turns band balance and phrase changes into a shared arch, counter-bend, bias and filament opening. Views never run their own springs.
- **Ribbon view**: panel or popup, sharing the shader and audio module. Audio-reactive colors stay within the selected palette and can be disabled for the original fixed gradient.

Priorities: passive monitor capture, bounded memory, stability, and visual quality at 200 × 40 pixels. Automated tests exercise the interfaces used by the application. Unperformed hardware checks must be identified explicitly. The interface and documentation are in English.
