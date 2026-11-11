# Changelog

## Unreleased

- A reference library in `~/Music/Lucky Reference/References`: open it with the LIBRARY button or by clicking an empty slot, search by name or folder, heart your favourites, and load with a double-click or Return. Subfolders are included.
- ADD TO LIBRARY on a reference loaded from elsewhere copies it into the library and keeps the slot playing.
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
