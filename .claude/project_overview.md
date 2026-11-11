# Lucky Reference: project overview

JUCE 9 (CMake + Ninja, fetched via FetchContent) VST3 reference plugin for the master track, a sibling of Lucky Seven (`../lucky-seven`). Licensed AGPLv3. The agreed v1 behaviour is in `docs/spec.md`.

## Layout
- `Source/PluginProcessor.*`: stereo in/out processor.
- `Source/PluginEditor.*`: editor.
- `Source/ui/`: `Theme` (Lucky palette, slot colours), `LookAndFeel` (Barlow typeface, flat buttons), both copied from Lucky Seven.
- `Tests/`: juce::UnitTest via CTest, category "Lucky Reference".
