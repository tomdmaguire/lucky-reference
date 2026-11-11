# Lucky Reference

Compare your mix against reference tracks at matched loudness. Lucky Reference sits last on your master track: load up to three references, choose the section of each to compare, and flip between your mix and a reference with one switch.

A sibling of [Lucky Seven](https://github.com/tomdmaguire/lucky-seven). Work in progress; see [`docs/spec.md`](docs/spec.md).

## Features

- **Three reference slots**: drop a WAV, AIFF, FLAC, MP3 or M4A file onto a slot, or click it to browse.
- **A loop for each reference**: a new reference loops its loudest 16 seconds. Drag across the waveform to choose another section, drag a loop edge to adjust it, double-click to play the whole track.
- **One A/B switch** with a short crossfade. It's also the first plugin parameter, so in Ableton you can add it to the device panel (Configure) and MIDI- or key-map it.
- **Loudness matching**: the reference is turned to your mix's BS.1770 loudness, measured over the section you're playing and the reference's loop. The readout shows the difference ("Ref −3.4 dB") and both LUFS values; turn Match off to hear the real gap.
- **Follows your transport**: references play when your DAW plays, and every reference restarts at its loop start when you press play.
- **Safe exports**: offline renders always contain your mix.

Put Lucky Reference last on your master, after any limiter or mastering chain (monitor correction such as Sonarworks can come after it).

## Building

Requirements: macOS 13 or newer, Xcode (with the command line tools selected) and CMake 3.25 or newer. Ninja is optional but faster.

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

`build/LuckyReferenceSnapshot <file.png> <empty|loaded|reference|measuring|nomatch>` renders the editor with generated references.

JUCE is fetched automatically by CMake. The build copies the VST3 into `~/Library/Audio/Plug-Ins/VST3`.

## Licence

AGPLv3. The bundled Barlow fonts are under the SIL Open Font License (`Resources/Fonts/OFL.txt`).
