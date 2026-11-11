# Lucky Reference: v1 spec

A reference plugin for the master track. Load up to three reference tracks, pick the section of each to compare, and flip between your mix and a reference at matched loudness.

## Positioning

- For mid-to-advanced producers and mix engineers.
- A listening tool, not an analyser. It makes the comparison fair (same loudness, same section, instant switch) and leaves the judgement to your ears.
- Nothing it doesn't need. Every control on screen is used in normal referencing.
- A sibling of Lucky Seven: same visual family, same "few controls, done well" approach.

## Problem it replaces

The manual routine: a group of reference tracks outside the mix, routed to the stereo out, soloed back and forth. That routine has no loudness matching, makes it slow to line up equivalent sections, and is easy to get wrong (forgetting a solo, references going through the master chain).

## Placement and signal flow

```
mix bus → [master chain, e.g. Ozone] → Lucky Reference → (monitor correction only) → out
```

- Must be the **last processor on the master**, except monitoring correction (Sonarworks, headphone correction), which should affect both signals equally.
- Input is always the mix. Output is either the mix (passed through untouched) or the selected reference.
- The mix is measured continuously, including while you are listening to a reference.
- The UI states the placement rule in the first-run hint.

## Features

### 1. Reference slots

- Three slots. Drag a file onto a slot, or click an empty slot to browse.
- Formats: WAV, AIFF, FLAC, MP3, M4A/AAC (CoreAudio decoders on macOS).
- Files are decoded on a background thread, resampled to the host rate, and held in memory as stereo. Mono files are duplicated to both channels; files with more than two channels use the first two.
- Maximum length: 15 minutes.
- Each slot has a pastel colour from the Lucky family palette.
- A slot whose file is missing on session load shows "Missing" with the filename; clicking it opens a browser to relocate it.
- Slots can be cleared and replaced.

### 2. Source switch (A/B)

- One large switch toggles between **Mix** and the **selected reference**.
- Clicking a slot selects it. If you are already listening to a reference, the output moves to the new one immediately.
- Every switch is a 30 ms equal-power crossfade.
- The switch is exposed as two host parameters, so it can be controlled from the DAW without opening the plugin window:
  - `reference` (on/off): the A/B switch itself. Off is the mix; on is the selected reference.
  - `slot` (1, 2, 3): which reference the switch plays.
- `reference` is the first parameter, and the plugin exposes only these plus `match`, so the host's generic controls stay uncluttered.
- In Ableton Live, configuring the plugin device adds these to the device panel as native controls, where they can be MIDI-mapped and key-mapped (for example the A/B switch on a single key or a footswitch).
- When the source is Mix and no crossfade is running, the output is bit-identical to the input.

### 3. Loop for each reference

- Each slot shows the full waveform of its file, in the slot's colour.
- Drag across the waveform to set the loop; drag either edge to adjust; double-click to clear it.
- When a file is first loaded, the loop is set to its loudest 16 seconds (about 8 bars at 120 BPM), where the arrangement is usually densest and the comparison most useful. It can then be changed freely; restoring a session keeps the saved loop.
- With no loop set (after clearing it), the whole track loops.
- Minimum loop length: 2 seconds.
- A 10 ms crossfade at the loop point avoids clicks.
- The loop defines three things at once: what plays, what is loudness-measured, and what the readout compares.

### 4. Playback

- References follow the host transport: they play when the host plays and stop when it stops.
- On transport start, every reference playhead jumps to its loop start. Pressing play on your chorus starts each reference on its chosen section.
- While the transport runs, all references advance in parallel, like tracks in a session, so flipping is instant.
- Clicking inside a loop moves that reference's playhead.
- No playback when the transport is stopped (see open questions).

### 5. Loudness matching

Measure: **ITU-R BS.1770 gated integrated loudness** (LUFS), the same measure on both sides.

- **Reference:** the integrated loudness of its loop region, computed from the in-memory file whenever the loop changes. It takes milliseconds, so it is ready before you listen.
- **Mix:** the integrated loudness of the mix since the measurement was last reset. It resets on transport start and on any jump in host position, so it always covers the section you are playing.
- **Gain:** the reference is turned by `mix LUFS − reference LUFS`. Only the reference is ever adjusted; the mix level is never touched.
- Gain changes ramp over 1 second. Because the integrated measure converges as more audio arrives, the gain settles naturally instead of riding the music.
- Until 3 seconds of non-silent mix have been measured, the last gain is held (0 dB for a fresh session) and the readout shows "Measuring".
- Gain is clamped to ±24 dB. A silent reference loop is not matched.
- A **Match** toggle turns matching off, so you can hear the real loudness gap (useful for checking a master).

### 6. Loudness readout

- Large: the gain applied to the active reference, e.g. **"Ref −3.4 dB"** (the reference was 3.4 dB louder than your mix and has been turned down).
- Small, underneath: both absolute values, e.g. "Mix −11.2 LUFS · Ref −7.8 LUFS".
- The large number animates to its new value as the measurement settles.
- With Match off, the large number shows the same difference but is not applied.

### 7. Export safety

- During offline rendering (export/bounce, detected by JUCE's `isNonRealtime()`), the plugin always outputs the mix untouched, whatever the source switch says.
- When bypassed, it passes the mix.

### 8. Session state

Saved with the project: file paths, the loop for each slot, and the `reference`, `slot` and `match` parameters. Reference audio is not embedded; it is reloaded from disk.

## UI

```
┌──────────────────────────────────────────────────────────┐
│ [Ab]  Lucky Reference                                ⋯   │
├──────────────────────────────────────────────────────────┤
│ 1 ▕▁▃▅▇▆▅▃▂[▅▇█▇▆▅]▃▂▁▂▃▅▆▅▃▂▁▕  Artist – Track    ♦      │
│ 2 ▕▁▂▃▅▆▇▆▅▄▃▂▁▂▃[▅▆▇▇▆]▅▃▂▁▕    Artist – Track          │
│ 3 ▕        drop a reference here          ▕              │
├──────────────────────────────────────────────────────────┤
│        ( MIX | REF )          Ref −3.4 dB                │
│                         Mix −11.2 LUFS · Ref −7.8 LUFS   │
│                                         [Match ●]        │
└──────────────────────────────────────────────────────────┘
```

- Top bar: the logo, the name, and a ⋯ menu (about, placement hint, clear all).
- Three slot rows: waveform with coloured loop region and playhead, file name, selection marker.
- Bottom strip: the A/B switch, the readout and the Match toggle.
- Fixed size with a resizable corner, like Lucky Seven.
- Knobs and buttons never take keyboard focus (hosts handle keys unreliably). Keyboard control goes through the host instead: key-map the `reference` parameter.

### Visual delight

- **Logo as the switch indicator:** a seven-segment "A" while you are on the mix and a lowercase "b" on a reference, flipping as you switch. The seven-segment display is the family brand; Lucky Seven shows "7", Lucky Reference shows "Ab".
- **Colour sweep:** the UI accent moves from a neutral mix colour to the selected reference's colour when you flip, timed with the audio crossfade.
- **Waveforms** that brighten inside the loop and dim outside it, with a moving playhead.
- **The readout number** rolls to new values rather than jumping.

## Technical notes

- JUCE 9, CMake + Ninja, JUCE fetched via FetchContent. Build setup, Theme and LookAndFeel copied from Lucky Seven as a starting point. Apple Silicon, VST3 only (AU can be added later in build config).
- Zero latency.
- **From JUCE:** plugin wrapper, parameters and state, audio file decoding (`AudioFormatManager` with CoreAudio formats), resampling, threads, GUI.
- **Written for this project:**
  - BS.1770 loudness meter: K-weighting (two biquads), 400 ms blocks with 75% overlap, −70 LUFS absolute gate and −10 LU relative gate. One implementation is used for both the offline reference measure and the live mix measure.
  - Reference player: loop playback with loop-point crossfades and the 30 ms source crossfade.
  - Waveform rendering and the loop editor.
- Threads:
  - Loader thread: decode, resample, build the waveform overview, measure the loop.
  - Audio thread: reads in-memory buffers handed over lock-free (atomic shared-pointer swap with deferred release off the audio thread), runs the mix meter, and applies gains and crossfades. No allocation or locks.
  - Message thread: UI, and reading meter values through atomics.

## Out of scope for v1

Tonal difference curve, timeline lock, mono or side solo, band solo and filters, blind test, stereo width and punch meters, beat-snapped loops, AU, standalone.

## Later

- Performance testing as done for Lucky Seven: CPU per instance, small buffer sizes, memory use with three long references.

## Decisions

1. **No audition while the transport is stopped** in v1.
2. **References run in parallel** with the transport, like tracks in a session.
3. **Codes:** manufacturer `Lcky` (shared with Lucky Seven), plugin `Lref`, bundle id `com.luckylabs.luckyreference`.
4. **Empty selected slot:** switching to it outputs silence, and the readout says the slot is empty.

## Tests

- The loudness meter matches the EBU Tech 3341 reference signals within ±0.1 LU.
- The mix passes through bit-identically when the source is Mix.
- Offline render outputs the mix whatever the source switch says.
- Matching: a reference played at a known offset settles to the expected gain within ±0.2 dB.
- Loop and source crossfades produce no discontinuities above a set threshold.
- State saves and restores slots, loops, and the `reference`, `slot` and `match` parameters.
- Toggling `reference` from the host with the editor closed switches the output.
