#pragma once

#include "LoopRegion.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <vector>

namespace lucky
{

/** A reference file decoded into memory as stereo at the host sample rate, with its waveform overview and loudness slices. Immutable once loaded. */
class ReferenceTrack
{
public:
    static constexpr double maximumSeconds = 15.0 * 60.0;
    static constexpr int overviewBuckets = 1024;

    /** Decodes, converts to stereo and resamples the file to sampleRate; returns nullptr and sets error when the file can't be used. */
    static std::shared_ptr<const ReferenceTrack> load (const juce::File& file, double sampleRate, juce::AudioFormatManager& formats,
                                                       juce::String& error, double maxSeconds = maximumSeconds);

    /** Builds a track from audio already at sampleRate (one or two channels). */
    ReferenceTrack (juce::File source, juce::AudioBuffer<float> stereoAudio, double sampleRate);

    const juce::File& getFile() const noexcept { return file; }
    juce::String getName() const { return file.getFileNameWithoutExtension(); }
    const juce::AudioBuffer<float>& getAudio() const noexcept { return audio; }
    double getSampleRate() const noexcept { return rate; }
    juce::int64 getLength() const noexcept { return audio.getNumSamples(); }
    double getLengthSeconds() const noexcept { return static_cast<double> (getLength()) / rate; }

    /** Peak level (0-1) of each of overviewBuckets equal slices of the track, for drawing the waveform. */
    const std::vector<float>& getOverview() const noexcept { return overview; }

    /** Gated integrated loudness in LUFS of the given loop, or -infinity when it is silent. */
    float getLoudness (const LoopRegion& loop) const;

    /** The loudest stretch of the given length, as a loop; unset when the track is no longer than that. */
    LoopRegion findLoudestSection (double seconds) const;

private:
    juce::File file;
    juce::AudioBuffer<float> audio;
    double rate;
    std::vector<float> overview;
    std::vector<double> slices;
};

using ReferenceTrackPtr = std::shared_ptr<const ReferenceTrack>;

} // namespace lucky
