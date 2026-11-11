# Lucky Reference

Compare your mix against reference tracks at matched loudness. Lucky Reference sits last on your master track: load up to three references, choose the section of each to compare, and flip between your mix and a reference with one switch.

A sibling of [Lucky Seven](https://github.com/tomdmaguire/lucky-seven). Work in progress; see [`docs/spec.md`](docs/spec.md).

## Building

Requirements: macOS 13 or newer, Xcode (with the command line tools selected) and CMake 3.25 or newer. Ninja is optional but faster.

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

JUCE is fetched automatically by CMake. The build copies the VST3 into `~/Library/Audio/Plug-Ins/VST3`.

## Licence

AGPLv3. The bundled Barlow fonts are under the SIL Open Font License (`Resources/Fonts/OFL.txt`).
