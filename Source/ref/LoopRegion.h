#pragma once

#include <juce_core/juce_core.h>

namespace lucky
{

/** A loop chosen inside a reference, stored in seconds so it survives a change of host sample rate. Unset means the whole track. */
struct LoopRegion
{
    static constexpr double minimumSeconds = 2.0;

    double startSeconds = 0.0;
    double endSeconds = 0.0;

    /** True when a loop has been chosen. */
    bool isSet() const noexcept { return endSeconds > startSeconds; }

    /** The loop as a sample range in a track of the given length, clamped inside it and at least minimumSeconds long when the track allows. */
    juce::Range<juce::int64> resolve (juce::int64 trackLength, double sampleRate) const noexcept
    {
        if (! isSet() || trackLength <= 0)
            return { 0, std::max<juce::int64> (0, trackLength) };

        const auto minimum = std::min (trackLength, static_cast<juce::int64> (std::ceil (minimumSeconds * sampleRate)));
        auto start = juce::jlimit<juce::int64> (0, trackLength, static_cast<juce::int64> (std::round (startSeconds * sampleRate)));
        auto end = juce::jlimit<juce::int64> (0, trackLength, static_cast<juce::int64> (std::round (endSeconds * sampleRate)));

        if (end - start < minimum)
        {
            end = std::min (trackLength, start + minimum);
            start = end - minimum;
        }

        return { start, end };
    }

    bool operator== (const LoopRegion&) const = default;
};

} // namespace lucky
