#pragma once

#include <juce_core/juce_core.h>

namespace lucky
{

/** A short label for a reference's file format, so a lossy reference is obvious: "MP3 · 128 kbps" for lossy files
    (bitrate from the file's header), "WAV" or "FLAC" for lossless ones, empty when the file has no extension. */
juce::String describeFileFormat (const juce::File& file);

} // namespace lucky
