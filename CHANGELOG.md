# Changelog

## Unreleased

- A reference library on its own LIBRARY tab: everything in `~/Music/Lucky Reference/References` plus any folders you add from the ⋯ menu. Search by name or folder, and load a track into reference 1, 2 or 3 with the buttons on its row. ADD FILES... copies files in. The logo or the REFS tab takes you back.
- ADD TO LIBRARY on a reference loaded from elsewhere copies it into the library and keeps it playing (shown once you've used the library).
- CLOSEST sorts the library by how close each track's tonal balance and punch are to your mix, over the last two minutes played.
- Hover a reference to remove it with ×, or press Delete.
- Closing the plug-in window switches back to your mix, so a reference never keeps playing out of sight. Switching from the DAW with the window closed still works.

## v0.1.0

First release.

- Three reference slots: drop a WAV, AIFF, FLAC, MP3 or M4A file onto a slot, or click it to browse. A new reference loops its loudest 16 seconds; drag across the waveform to choose another section.
- One A/B switch in the middle of the window, with a short crossfade. Press ← for your mix, → for the reference and 1/2/3 to choose a slot. The switch is also the first plug-in parameter, so it can be added to Ableton's device panel and MIDI- or key-mapped.
- Loudness matching (BS.1770): the reference is turned to your mix's loudness over the last 10 seconds played. The readout says how your mix compares ("3.4 dB quieter") with both LUFS values; turn LEVEL MATCH off to hear the real gap.
- A format badge beside each reference's name, with the bitrate for lossy files.
- Keyboard shortcuts: left/right arrows switch, 1/2/3 pick a reference, shown as keycaps under the switch.
- References follow your DAW's transport and restart at their loop start when you press play.
- Exports always contain your mix.
- Window size from 75% to 200%.
- Clicking an empty slot browses from the folder of your other references, or the last folder you used.
