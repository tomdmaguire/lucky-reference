# Lessons

Carried over from Lucky Seven where they apply.

## Async-loading test fixtures
- **Rule:** when waiting on async state, wait for the exact expected value (e.g. a buffer length), not "non-zero".

## Don't disturb a live DAW session
- **Rule:** while the DAW is in use for testing, build with `nice -n 19` and limited `-j`, build only test/tool targets, and install (full build) only after the host has quit.
- **Check first:** run `pgrep -lf "Ableton Live"` before any full build; the plugin build installs itself, so a full build while the host has the plugin loaded swaps the binary under it.

## Match a sibling product's UI conventions before building
- **Mistake:** Lucky Reference's first UI copied Lucky Seven's colours but not its structure (outlined panels, top bar separators, three-dot icon, Window size menu, scaling), so it felt like a different product.
- **Rule:** when building a sibling, open its screenshots and editor code first and list the conventions to carry over; review snapshots side by side with the sibling's.

## Benchmark whole-file work on realistic lengths
- **Mistake:** JUCE's WindowedSincInterpolator took 0.9 s for a 24 s file (about 9 s per song); tests only used short signals, so nothing flagged it.
- **Rule:** time every whole-file step (decode, resample, analysis) on a 4-minute file at a mismatched sample rate before shipping; keep the benchmark tool.

## Never play a level-matched signal unmatched
- **Mistake:** the reference played at its full level until the mix had been measured, so every fresh session started with a loud commercial master.
- **Rule:** when a gain depends on a measurement that isn't ready, keep the signal silent (or at a safe level) and fade it in once the measurement exists.

## User-facing numbers need plain words
- **Mistake:** "Ref −3.4 dB" was correct but hard to read once on screen: it didn't say what was compared or which way.
- **Rule:** label what is compared with what ("YOUR MIX VS REF 2") and say the direction in words ("3.4 dB quieter"), with the raw values as detail.
