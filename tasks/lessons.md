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
## Feedback belongs to what changed
- **Mistake:** picking a reference while on the mix sent a white ripple from the MIX side, and LEVEL MATCH took the mix colour, though it only affects the reference.
- **Rule:** a ripple, glow or colour comes from the control that just changed, in that thing's colour. A control that only affects one thing always wears that thing's colour.

## Shortcut hints must look like keys
- **Mistake:** "1 2 3 SLOTS" read as labels, not number keys, and used an internal word.
- **Rule:** draw shortcuts as keycaps and label them with the product's own words ("reference", not "slot").

## Never keep a pointer into a temporary
- **Mistake:** `if (const auto* list = JSON::parse (file).getArray())` and `const auto* entry = findEntry (library.getEntries(), key)` both pointed into a value destroyed at the end of the statement, so favourites silently failed to load and a test read garbage.
- **Rule:** when a call returns by value (`var`, `std::vector`, a `unique_ptr` from a factory), store it in a named local before taking a pointer or reference into it or into anything it owns. `makeProcessor()->getLibrary()` crashed the same way: the temporary processor was the shared library's last owner.

## Keep a browser independent of hidden state
- **Mistake:** the library loaded into "the selected slot" (shown as "INTO REF 1"), so what a double-click did depended on state chosen on another screen; SUGGEST and favourites added features nobody needed.
- **Rule:** a browser says where things go on the thing itself (per-row 1 2 3 buttons), every panel has an obvious way back (tabs plus the logo, as in Lucky Seven), and the main screen works without the power-user feature.
