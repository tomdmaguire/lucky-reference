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

- Three slots. Drag a file onto a slot, or click an empty slot to browse for one. The reference library is a separate tab (see Reference library); nothing on the REFS tab opens it, so the main screen works without it.
- Formats: WAV, AIFF, FLAC, MP3, M4A/AAC (CoreAudio decoders on macOS).
- Files are decoded on a background thread, resampled to the host rate, and held in memory as stereo. Mono files are duplicated to both channels; files with more than two channels use the first two.
- Maximum length: 15 minutes.
- Each slot has a colour from the Lucky family palette: slot 1 is Lucky Seven's gold, slot 2 pink, slot 3 green.
- Browsing for a file starts in this slot's folder, else the folder of a reference in another slot (usually the project's), else the last folder a reference came from, else Music. Plugins can't ask the host for the project folder, so this is the closest guess.
- References are linked by path, not copied into the project, so Ableton's Collect All and Save doesn't gather them. Keeping references in the library gives them a stable home (see Reference library). A slot whose file is missing on session load says so with the filename; clicking it opens a browser to relocate it.
- Closing the plugin window switches back to the mix, so a reference never keeps playing out of sight. Switching from the host with the window closed still works.
- Slots can be cleared and replaced. Hovering a loaded slot shows a × to remove its reference; Delete or Backspace removes the selected one, and the slot's right-click menu has "Remove reference".

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
- No playback when the transport is stopped.

### 5. Loudness matching

Measure: **ITU-R BS.1770 gated integrated loudness** (LUFS), the same measure on both sides.

- **Reference:** the integrated loudness of its loop region, computed from the in-memory file whenever the loop changes. It takes milliseconds, so it is ready before you listen.
- **Mix:** the gated loudness of the last 10 seconds of the mix, updated every 100 ms. It resets on transport start and on any jump in host position (not on a host loop wrap), so it covers the section you are playing, and turning your mix up or down moves the readout and the matched reference within about 10 seconds.
- **Gain:** the reference is turned by `mix LUFS − reference LUFS`. Only the reference is ever adjusted; the mix level is never touched.
- Gain changes ramp over 1 second, so the gain settles smoothly instead of riding the music.
- Matching starts once 1.5 seconds of non-silent mix have been measured. Before the first estimate of a session, a selected reference stays silent and the readout shows "Measuring" while playing ("Waiting for your mix" until you press play); it then fades in at the matched level, so it never plays at its full, unmatched level. After that, the last estimate is held across stops and jumps until the new section has been measured.
- Gain is clamped to ±24 dB. A silent reference loop is not matched.
- A **LEVEL MATCH** toggle turns matching off, so you can hear the real loudness gap (useful for checking a master).

### 6. Loudness readout

- Plain words: a small label **"YOUR MIX VS REF 2"**, then a large **"3.4 dB quieter"**, "2.1 dB louder" or "Same loudness" (under 0.1 dB).
- Small, underneath: both absolute values, e.g. "Mix −11.2 LUFS · Ref −7.8 LUFS".
- The large number rolls to its new value as the measurement settles.
- With LEVEL MATCH off, the same difference is shown with "Levels not matched".

### 7. Export safety

- During offline rendering (export/bounce, detected by JUCE's `isNonRealtime()`), the plugin always outputs the mix untouched, whatever the source switch says.
- When bypassed, it passes the mix.

### 8. Session state

Saved with the project: file paths, the loop for each slot, and the `reference`, `slot` and `match` parameters. Reference audio is not embedded; it is reloaded from disk.

### 9. Reference library

- The main folder, `~/Music/Lucky Reference/References`, is created if missing. More folders can be added from the three-dot menu (**Library folders** › "Add a folder...", as in Lucky Seven); they are indexed where they are, never copied, and each can be shown in Finder or removed (its files stay put). The added folders are kept in `folders.json` next to the cache. Subfolders are included and shown as a quiet label next to each track (an added folder's tracks show its name).
- Shared by every instance. A rescan lists the folders (when the library opens, then every 5 s while it is open, or from "Rescan all folders"), and a low-priority thread reads each new or changed file's format and length once. The results are cached in `~/Library/Application Support/Lucky Reference/library-cache.json`, keyed by path, size and modification date.
- **LIBRARY tab:** REFS and LIBRARY tabs sit next to the three-dot menu, as Lucky Seven's MAP and LIST do, and the logo returns to REFS. The library covers the slot rows; the stage stays live, so you can keep switching while you browse. It doesn't depend on which slot is selected.
  - Search box: every word must appear in the track name or its folder. It only takes keyboard focus when clicked, and clicking anywhere else in the library gives focus back to the list, so keys the library doesn't use (such as the host's space bar) still reach the host. Up and down move through the results, and Escape clears the search, then returns to REFS.
  - Each row: name, folder, format badge, length, and **1 2 3** buttons in the slots' colours. Clicking one loads the track into that slot and stays in the library, so all three can be filled in a row; a filled button shows the slot already holds the track. If the slot already holds a different track, a card over the list asks first ("Replace “Track” in reference 2?"), adding that it isn't in your library when that's so, since it would be hard to find again; Enter replaces and Escape cancels. With the list focused, the 1, 2 and 3 keys do the same.
  - Double-click or Return loads into the first empty slot; when all three are full, the header says to use 1, 2 or 3.
  - **ADD FILES...** (or dropping files on the library) copies audio files into the main folder in the background and stays in the library.
  - When the library is empty, it says where to put tracks and offers ADD FILES... and SHOW IN FINDER.
- **Add to library:** once you have opened the library, a reference loaded from outside it shows a small ADD TO LIBRARY button on its slot (also in the slot's menu). It copies the file into the main folder on a background thread and points the slot at the copy, keeping the audio and loop as they are. Nothing is ever copied without asking. A file with the same name and size already in the library is reused; a different file with the same name gets a number.

### 10. Closest to your mix

- **Tonal profile:** the same analysis for a library track and for the mix. A mono sum is split into 27 third-octave bands from 31.5 Hz to 12.5 kHz (8192-point Hann FFT frames with 50% overlap at 48 kHz; 16384 points above 64 kHz), averaged in energy, and expressed in dB relative to the average band, so loudness has no effect. The bands stop below 16 kHz on purpose: lossy files cut the top end there, and a lossy reference shouldn't count as different just for that. **Punch** is the median peak-to-RMS ratio of one-second windows: limiting lowers it.
- **Gating:** audio counts only if the second around it is above -70 dBFS and within 10 dB of the average, so silence, fades and quiet intros are ignored while the gaps between drum hits still count.
- **Library side:** each track is analysed once by the library thread, reading the file in chunks (up to 15 minutes), and the profile is cached with the track's details. A 4-5 minute track takes 0.3-0.45 s.
- **Mix side:** while the transport plays, the audio thread copies the mono mix into a preallocated lock-free FIFO next to the loudness meter (no allocation or locks). The processor's timer drains it twice a second into an analyser covering the last two minutes played, so the profile follows the mix as it changes. It isn't reset when playback restarts. It counts as ready after 20 seconds of gated playback.
- **Distance:** the RMS difference across the bands combined with the punch difference, in dB. Different productions in one genre are typically 3-7 dB apart. The match bar shows `exp(-distance / 9 dB)`, as a bar with no percentage, so it reads as a ranking rather than a grade.
- **CLOSEST** (next to A-Z in the library header) sorts by match, closest first, still filtered by the search. Until the mix is ready it shows no list, only "Play your mix for 20 seconds" with a progress bar; the ranked list appears once it is ready. The order never moves on its own. Twice a second the library checks whether the mix as it is now would put different tracks in the top three places; if so, the line above the list says "Your top matches have changed" with an UPDATE button, and only clicking it (or CLOSEST, or coming back to the library) re-ranks. Changes further down the list are ignored, and the two-minute mix window stays behind the scenes.
- **Why 20 seconds:** `LuckyReferenceProfile --stability` treats stretches of real tracks as a mix. On a six-track library, stretches spread through each song were on average 3.2 dB from the whole song after 5 s, 2.6 dB after 10 s, 2.2 dB after 20 s and 1.4 dB after 60 s. Different songs in one genre are 3-7 dB apart, so less than 20 s can mistake one song for another. More listening keeps improving the match. The loudest stretch alone is *further* from the whole song (3.4 dB at 20 s), because a chorus is brighter and denser than the song overall, so "play the loudest part" is the wrong advice while library tracks are profiled whole.
- There is no automatic fill: picking from the ranked list with the 1 2 3 buttons is quick and keeps the choice yours.

## UI

```
┌──────────────┬─────────────────────────────┬─────────┬───┐
│ [Ab] LUCKY REFERENCE                │ REFS │ LIBRARY │ ⋯ │
├──────────────┴─────────────────────────────┴─────────┴───┤
│ ┌ 1 ▁▃▅▇▆▅▃▂[▅▇█▇▆▅]▃▂▁▂▃▅▆▅▃▂▁   Artist – Track  16 s ┐ │
│ ┌ 2 ▁▂▃▅▆▇▆▅▄▃▂▁▂▃[▅▆▇▇▆]▅▃▂▁     Artist – Track        ┐ │
│ ┌ 3        drop a reference here                        ┐ │
├──────────────────────────────────────────────────────────┤
│ YOUR MIX VS REF 1     .-~~~~~~~~~~-.                     │
│ 3.4 dB quieter       (  ( MIX|REF )  )     [● LEVEL MATCH]│
│ Mix −11.2 · Ref −7.8  '-~~~~~~~~~~-'                     │
│              [←][→] SWITCH   [1][2][3] REFERENCE         │
└──────────────────────────────────────────────────────────┘
```

- Structure copied from Lucky Seven: a fixed 900×600 view scaled to the window (75–200%, fixed aspect ratio), a **Window size** menu (100/125/150/175%) and a remembered scale; the top bar has separators, the REFS and LIBRARY tabs and the three-dot menu (window size, library folders, placement hints, remove all references). The logo returns to REFS.
- Three outlined slot rows (100 px at 100% scale): waveform with coloured loop region, a playhead on the selected slot only (the others keep time silently), file name, a format badge, loop length.
- The format badge makes a lossy reference obvious: "MP3 · 192 kbps" (bitrate from the file header) for MP3, AAC, Ogg and Opus, and lossy M4A; just "WAV", "AIFF", "FLAC" or lossless "M4A" (ALAC) otherwise. A lossy file has a softer top end, which is worth knowing before matching your mix to it.
- The stage: readout on the left, the A/B switch in the centre with the radar ripples behind it, LEVEL MATCH on the right, and a shortcut hint underneath drawn as keycaps. The readout headline, the switch and LEVEL MATCH share one centre line.
- LEVEL MATCH only affects the reference, so it always takes the selected reference's colour, as does the "Levels not matched" note.
- **Keyboard:** ← plays the mix, → plays the reference, 1/2/3 choose the slot, Delete removes the selected slot's reference. The shortcut hint hides while the library is shown. The window takes keyboard focus when shown and when clicked; buttons never take it. The host's own key mapping of the `reference` parameter also works.

### Visual delight

- **Radar ripples:** Lucky Seven's map ripples applied to the switch. Each hit in the audible signal (its 25 ms level jumping relative to the mix loudness) sends a faint circle out from the knob that grows and fades over 2.6 s, so a few overlap like a radar; a soft glow behind the knob follows the level. Every switch sends a stronger ripple in the new source's colour. Meant to be felt more than watched. (A halo design around the switch was tried and dropped in favour of this.)
- **Springy switch:** the knob slides with a slight overshoot and stretch.
- **Logo as the switch indicator:** a seven-segment "A" while you are on the mix and a lowercase "b" on a reference. The seven-segment display is the family brand; Lucky Seven shows "7", Lucky Reference shows "Ab".
- **Colour sweep:** the UI accent moves from a neutral mix colour to the selected reference's colour when you flip.
- **Waveforms** that brighten inside the loop and dim outside it, with a moving playhead.

## Technical notes

- JUCE 9, CMake + Ninja, JUCE fetched via FetchContent. Build setup, Theme and LookAndFeel copied from Lucky Seven as a starting point. Apple Silicon, VST3 only (AU can be added later in build config).
- Zero latency.
- **From JUCE:** plugin wrapper, parameters and state, audio file decoding (`AudioFormatManager` with CoreAudio formats), threads, GUI.
- **From macOS:** sample-rate conversion with AudioToolbox's `AudioConverter` (normal complexity, max quality); a 4-minute song loads in about 0.3 s.
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

- A shared Lucky UI library for both plugins: theme, look and feel, seven-segment glyphs, menu icon and the scaling editor are currently copied from Lucky Seven.

## Performance (measured)

- Engine, 48 kHz, three 4-minute references: average 0.05-0.09% of each block's time budget; p99.9 under 3%; worst block under 6% at 64 samples and under 15% at 32 samples (`tools/Benchmark.cpp`). Re-measured with the mix tap for closest matching: average 0.04-0.08%, p99.9 at most 0.2%, worst block 2.7% at 32 samples.
- Library analysis of a 4-5 minute track: 0.3-0.45 s on the library thread (`tools/ProfileTool.cpp`).
- Whole plugin through a VST3 host with no references playing: 0.02% of a core at 64 samples.
- Loading a 4-minute 44.1 kHz file at 48 kHz: 0.3 s. Memory: about 92 MB per 4-minute reference; loading briefly needs about twice one track.
- pluginval strictness 10: all 25 test groups pass. No leaks in the processor tests.

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
- Closing the editor switches back to the mix.
- The library finds tracks in subfolders, reuses cached details and re-reads changed files, drops deleted files, indexes added folders in place and remembers them, matches searches on every word, and imports a file once (numbering a different file with the same name).
- Adding a reference to the library keeps its audio and loop and saves the library copy's path with the session.
- Tonal profiles ignore loudness, show a dull track as dull, ignore silence and quiet passages, show limiting as lower punch, forget audio outside the window, don't depend on block size, and rank a mix's matching reference first. Cached profiles match a fresh analysis.
- The mix profile needs 20 s of playback and ignores a stopped transport; the library track closest to the mix that played ranks first.
