#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace lucky
{

/** Converts a whole stereo buffer to another sample rate with macOS's high-quality AudioConverter, keeping it aligned in time.
    Returns the source itself when the rates already match, and an empty buffer if the conversion fails. */
juce::AudioBuffer<float> resampleStereo (juce::AudioBuffer<float> source, double sourceRate, double targetRate);

} // namespace lucky
