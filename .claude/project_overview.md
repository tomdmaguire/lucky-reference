# Lucky Reference: project overview

JUCE 9 (CMake + Ninja, fetched via FetchContent) VST3 reference plugin for the master track, a sibling of Lucky Seven (`../lucky-seven`). Licensed AGPLv3. The agreed v1 behaviour is in `docs/spec.md`.

## Layout
- `Source/PluginProcessor.*`: parameters `reference` (the A/B switch, first so it is easy to put on Ableton's device panel), `slot` (1-3), `match`. Slot file paths and loops live as `Slot` children of the APVTS state, loops in seconds so they survive a sample-rate change. Loads files through `ReferenceLoader` and, for a newly loaded file, sets the loop to its loudest 16 s. Offline renders (`isNonRealtime`) return the mix untouched.
- `Source/PluginEditor.*`: top bar (seven-segment A/b logo, lit letter follows the source), three `SlotRow`s, bottom strip with `SourceSwitch`, the readout ("Ref -3.4 dB" plus both LUFS values) and the Match toggle. Accent colour sweeps to the active reference's colour.
- `Source/dsp/LoudnessMeter.*`: BS.1770-4 gated integrated loudness. Streaming (0.05 LU histogram, no audio-thread allocation) plus offline helpers working on 100 ms K-weighted slice energies.
- `Source/dsp/ReferenceEngine.*`: per block: adopt new slot sources, follow the transport (start restarts every reference at its loop start and resets the mix meter; a jump resets the meter; a host loop wrap doesn't), measure the mix while playing, set each slot's gain to `mix LUFS - loop LUFS` (clamped ±24 dB, ramped 1 s, held until 3 s of mix is measured), 30 ms equal-power source crossfade. Bit-identical pass-through while on the mix.
- `Source/dsp/ReferencePlayer.*`: one per slot; loop playback with a 10 ms crossfade at the loop point. `Handoff.h`: lock-free message-to-audio handoff with a retire queue.
- `Source/ref/`: `ReferenceTrack` (decoded stereo at host rate, overview peaks, loudness slices, loudest-section search), `ReferenceLoader` (background decode, newest request per slot wins), `LoopRegion`.
- `Source/ui/`: `SlotRow`, `SourceSwitch`, `SegmentGlyph`, `Theme` (slot colours), `LookAndFeel` (from Lucky Seven; adds a `toggle` outline button style).
- `Tests/`: `LuckyReferenceTests` (DSP and loading, category "Lucky Reference") and `LuckyReferenceProcessorTests` (links the plugin; parameters, export safety, state, auto loop).
- `tools/Snapshot.cpp`: renders the editor to PNG with generated references (`empty|loaded|reference|measuring|nomatch`).

## Notes
- Tests generate AAC fixtures with `/usr/bin/afconvert`.
- The plugin build uses LTO and is universal, so linking takes over a minute; build `LuckyReferenceTests` alone for quick DSP iteration.
