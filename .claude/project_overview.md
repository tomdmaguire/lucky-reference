# Lucky Reference: project overview

JUCE 9 (CMake + Ninja, fetched via FetchContent) VST3 reference plugin for the master track, a sibling of Lucky Seven (`../lucky-seven`). Licensed AGPLv3. The agreed v1 behaviour is in `docs/spec.md`.

## Layout
- `Source/PluginProcessor.*`: parameters `reference` (the A/B switch, first so it is easy to put on Ableton's device panel), `slot` (1-3), `match`. Slot file paths and loops live as `Slot` children of the APVTS state, loops in seconds so they survive a sample-rate change. Loads files through `ReferenceLoader` and, for a newly loaded file, sets the loop to its loudest 16 s. Offline renders (`isNonRealtime`) return the mix untouched.
- `Source/PluginEditor.*`: `LuckyReferenceEditor` scales a fixed 900x600 `LuckyReferenceView` (Lucky Seven's pattern: Window size menu, scale saved by `ui/Settings.h`). The view has the top bar (seven-segment A/b logo, separators, three-dot menu), three `SlotRow`s, and a stage: readout ("YOUR MIX VS REF n / 3.4 dB quieter" plus LUFS), `RadarRipples` behind the `SourceSwitch` (the readout is a `PaintLayer` above them), LEVEL MATCH in the selected slot's colour, and a keycap shortcut hint (`paintShortcutHint`). Keys: left/right mix/reference, 1-3 slot.
- `Source/dsp/LoudnessMeter.*`: BS.1770-4 gated loudness. Streaming over a rolling 10 s window of 400 ms blocks (plus momentary), and offline helpers working on 100 ms K-weighted slice energies.
- `Source/dsp/ReferenceEngine.*`: per block: adopt new slot sources, follow the transport (start restarts every reference at its loop start and resets the mix meter; a jump resets the meter; a host loop wrap doesn't), measure the mix while playing, set each slot's gain to `mix LUFS - loop LUFS` (clamped ±24 dB, ramped 1 s; matching starts after 1.5 s, and a selected reference is silent until the first estimate, then fades in), 30 ms equal-power source crossfade. Exposes mix momentary loudness, applied gains and play state for the radar. Bit-identical pass-through while on the mix.
- `Source/dsp/ReferencePlayer.*`: one per slot; loop playback with a 10 ms crossfade at the loop point. `Handoff.h`: lock-free message-to-audio handoff with a retire queue.
- `Source/ref/`: `ReferenceTrack` (decoded stereo at host rate, overview peaks, loudness slices, momentary loudness at a position, loudest-section search), `Resampler` (macOS AudioConverter), `ReferenceLoader` (background decode, newest request per slot wins), `LoopRegion`.
- `Source/ui/`: `SlotRow`, `SourceSwitch` (spring knob), `RadarRipples` (Lucky Seven-style ripples and glow from the knob), `SegmentGlyph`, `Settings`, `Theme` (slot 1 gold, 2 pink, 3 green), `LookAndFeel` (from Lucky Seven; adds a `toggle` outline button style).
- `Tests/`: `LuckyReferenceTests` (DSP and loading, category "Lucky Reference") and `LuckyReferenceProcessorTests` (links the plugin; parameters, export safety, state, auto loop).
- `tools/Benchmark.cpp`: engine CPU per block at 32-512 samples. `packaging/` + `.github/workflows/`: Lucky Seven's release pipeline adapted for VST3 only (tag `v*` -> pkg + zip).
- `tools/Snapshot.cpp`: renders the editor to PNG with generated references (`empty|loaded|reference|measuring|nomatch|radar`). `tools/LoadBenchmark.cpp`: times loading a file (or `--synthetic` 4-minute song).

## Notes
- Tests generate AAC fixtures with `/usr/bin/afconvert`.
- The plugin build uses LTO and is universal, so linking takes over a minute; build `LuckyReferenceTests` alone for quick DSP iteration.
