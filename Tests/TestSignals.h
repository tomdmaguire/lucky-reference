#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace lucky::test
{

/** A stereo 1 kHz sine with the given peak level in dBFS. */
inline juce::AudioBuffer<float> sine (float dbfs, double seconds, double sampleRate, int channels = 2, double hz = 1000.0)
{
    juce::AudioBuffer<float> buffer (channels, static_cast<int> (std::round (seconds * sampleRate)));
    const auto amplitude = juce::Decibels::decibelsToGain (dbfs, -200.0f);
    const auto step = juce::MathConstants<double>::twoPi * hz / sampleRate;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        for (int ch = 0; ch < channels; ++ch)
            buffer.setSample (ch, i, amplitude * static_cast<float> (std::sin (step * i)));
    return buffer;
}

/** Appends b to a, channel by channel. */
inline juce::AudioBuffer<float> concatenate (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    juce::AudioBuffer<float> joined (a.getNumChannels(), a.getNumSamples() + b.getNumSamples());
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
    {
        joined.copyFrom (ch, 0, a, ch, 0, a.getNumSamples());
        joined.copyFrom (ch, a.getNumSamples(), b, ch, 0, b.getNumSamples());
    }
    return joined;
}

/** A fresh, empty folder for one test's files. */
inline juce::File scratchFolder (const juce::String& name)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuckyReferenceTests").getChildFile (name);
    folder.deleteRecursively();
    folder.createDirectory();
    return folder;
}

/** Writes the buffer as a 24-bit WAV file. */
inline void writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    file.deleteFile();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    const auto options = juce::AudioFormatWriterOptions()
                             .withSampleRate (sampleRate)
                             .withNumChannels (buffer.getNumChannels())
                             .withBitsPerSample (24);
    if (auto writer = format.createWriterFor (stream, options))
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

} // namespace lucky::test
