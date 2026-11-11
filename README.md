# Lucky Reference

Compare your mix against reference tracks at matched loudness. Lucky Reference sits last on your master track: load up to three references, choose the section of each to compare, and flip between your mix and a reference with one switch.

A sibling of [Lucky Seven](https://github.com/tomdmaguire/lucky-seven). Work in progress; see [`docs/spec.md`](docs/spec.md).

## Features

- **Three reference slots**: drop a WAV, AIFF, FLAC, MP3 or M4A file onto a slot, or click it to browse.
- **A loop for each reference**: a new reference loops its loudest 16 seconds. Drag across the waveform to choose another section, drag a loop edge to adjust it, double-click to play the whole track.
- **One A/B switch** in the middle, with a short crossfade. Press ← for your mix, → for the reference and 1/2/3 to choose a slot. It's also the first plugin parameter, so in Ableton you can add it to the device panel (Configure) and MIDI- or key-map it.
- **Loudness matching**: the reference is turned to your mix's BS.1770 loudness, measured over the last 10 seconds you've played and the reference's loop. The readout says how your mix compares ("3.4 dB quieter") with both LUFS values; turn LEVEL MATCH off to hear the real gap.
- **Loudness rings** around the switch follow each signal's loudness and sit together when levels match.
- **Window size** from 75% to 200%: drag the corner or pick a size from the **⋯** menu.
- **Follows your transport**: references play when your DAW plays, and every reference restarts at its loop start when you press play.
- **Safe exports**: offline renders always contain your mix.

References are linked by path, not copied into your project, so keep them where they are (or relocate them from the slot if you move them).

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
