# Lucky Reference

Compare your mix against reference tracks at matched loudness. Lucky Reference sits last on your master track: load up to three references, choose the section of each to compare, and flip between your mix and a reference with one switch.

**[Website and download](https://tomdmaguire.github.io/lucky-reference/)** · a sibling of [Lucky Seven](https://github.com/tomdmaguire/lucky-seven) · product spec in [`docs/spec.md`](docs/spec.md)

![Lucky Reference with three references loaded, reference 2 playing and the readout saying the mix is 4.0 dB quieter](docs/assets/reference.png)

## Install

Download [**Lucky-Reference.pkg**](https://github.com/tomdmaguire/lucky-reference/releases/latest/download/Lucky-Reference.pkg) and open it. It installs the VST3 (Apple Silicon and Intel) into `/Library/Audio/Plug-Ins/VST3`.

Lucky Reference isn't signed with a paid Apple developer certificate, so macOS won't open the installer the first time. Click **Done**, then go to **System Settings → Privacy & Security**, scroll down and click **Open Anyway**.

A zip for installing by hand is also on the [releases page](https://github.com/tomdmaguire/lucky-reference/releases/latest); its `INSTALL.txt` explains the steps.

## Features

- **Three reference slots**: drop a WAV, AIFF, FLAC, MP3 or M4A file onto a slot, or click it to browse.
- **A loop for each reference**: a new reference loops its loudest 16 seconds. Drag across the waveform to choose another section, drag a loop edge to adjust it, double-click to play the whole track.
- **One A/B switch** in the middle, with a short crossfade. Press ← for your mix, → for the reference and 1/2/3 to choose a slot. It's also the first plugin parameter, so in Ableton you can add it to the device panel (Configure) and MIDI- or key-map it.
- **Loudness matching**: the reference is turned to your mix's BS.1770 loudness, measured over the last 10 seconds you've played and the reference's loop. The readout says how your mix compares ("3.4 dB quieter") with both LUFS values; turn LEVEL MATCH off to hear the real gap.
- **Format badge** next to each reference's name, with the bitrate for lossy files ("MP3 · 192 kbps"), so you know when you're comparing against a lower-quality file.
- **Keyboard**: left/right arrows switch between mix and reference, 1/2/3 pick a reference; the keys are shown under the switch.
- **Window size** from 75% to 200%: drag the corner or pick a size from the **⋯** menu.
- **Follows your transport**: references play when your DAW plays, and every reference restarts at its loop start when you press play.
- **Safe exports**: offline renders always contain your mix.
- **Light**: about 0.1% of the per-block time on average at 48 kHz, with the worst block under 6% at a 64-sample buffer. A 4-minute reference loads in about 0.3 s and holds about 90 MB.

References are linked by path, not copied into your project, so keep them where they are (or relocate them from the slot if you move them).

Put Lucky Reference last on your master, after any limiter or mastering chain (monitor correction such as Sonarworks can come after it).

## Building

Requirements: macOS 13 or newer, Xcode (with the command line tools selected) and CMake 3.25 or newer. Ninja is optional but faster.

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Tools

- `build/LuckyReferenceSnapshot <file.png> <empty|loaded|reference|measuring|nomatch|radar>` renders the editor with generated references.
- `build/LuckyReferenceBench_artefacts/Release/LuckyReferenceBench` measures the engine's CPU use at 32-512 sample buffers, on the mix, on a reference and while flipping between them. Run it with nothing else busy (a DAW open skews the numbers).
- `build/LuckyReferenceLoadBenchmark_artefacts/Release/LuckyReferenceLoadBenchmark <file | --synthetic> [host rate]` times decoding, resampling and analysis.

### Release checks

Before tagging a release, on the installed build:

```sh
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --validate-in-process --validate ~/Library/Audio/Plug-Ins/VST3/"Lucky Reference.vst3"
lipo -info ~/Library/Audio/Plug-Ins/VST3/"Lucky Reference.vst3"/Contents/MacOS/"Lucky Reference"   # x86_64 arm64
leaks --atExit -- build/LuckyReferenceProcessorTests
```

Then add the version's entry to `CHANGELOG.md` and push a tag such as `v0.1.0`: GitHub Actions builds, tests, ad-hoc signs and publishes `Lucky-Reference.pkg` and a zip.

JUCE is fetched automatically by CMake. The build copies the VST3 into `~/Library/Audio/Plug-Ins/VST3`.

## Licence

AGPLv3. The bundled Barlow fonts are under the SIL Open Font License (`Resources/Fonts/OFL.txt`).
