#include "ReferenceTrack.h"
#include "dsp/LoudnessMeter.h"

namespace lucky
{

namespace
{
juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>& source, double sourceRate, double targetRate)
{
    if (std::abs (sourceRate - targetRate) < 0.5)
        return source;

    const auto ratio = sourceRate / targetRate;
    const auto latency = static_cast<int> (juce::WindowedSincInterpolator::getBaseLatency());
    const auto outputLength = static_cast<int> (std::ceil (source.getNumSamples() / ratio));
    const auto skip = static_cast<int> (std::round (latency / ratio));
    const auto padded = source.getNumSamples() + latency * 2 + 8;

    juce::AudioBuffer<float> input (2, padded);
    input.clear();
    juce::AudioBuffer<float> output (2, outputLength + skip);

    for (int ch = 0; ch < 2; ++ch)
    {
        input.copyFrom (ch, 0, source, ch, 0, source.getNumSamples());
        juce::WindowedSincInterpolator interpolator;
        interpolator.process (ratio, input.getReadPointer (ch), output.getWritePointer (ch), output.getNumSamples());
    }

    juce::AudioBuffer<float> aligned (2, outputLength);
    for (int ch = 0; ch < 2; ++ch)
        aligned.copyFrom (ch, 0, output, ch, skip, outputLength);
    return aligned;
}

juce::AudioBuffer<float> toStereo (juce::AudioBuffer<float> audio)
{
    if (audio.getNumChannels() == 2)
        return audio;

    juce::AudioBuffer<float> stereo (2, audio.getNumSamples());
    stereo.copyFrom (0, 0, audio, 0, 0, audio.getNumSamples());
    stereo.copyFrom (1, 0, audio, audio.getNumChannels() > 1 ? 1 : 0, 0, audio.getNumSamples());
    return stereo;
}
} // namespace

std::shared_ptr<const ReferenceTrack> ReferenceTrack::load (const juce::File& file, double sampleRate, juce::AudioFormatManager& formats,
                                                            juce::String& error, double maxSeconds)
{
    const std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = file.existsAsFile() ? "Can't read this file" : "File not found";
        return nullptr;
    }

    const auto length = static_cast<int> (std::min<juce::int64> (reader->lengthInSamples,
                                                                 static_cast<juce::int64> (maxSeconds * reader->sampleRate)));
    const auto channels = static_cast<int> (std::min<unsigned int> (reader->numChannels, 2));
    if (length <= 0 || channels <= 0)
    {
        error = "This file has no audio";
        return nullptr;
    }

    juce::AudioBuffer<float> decoded (channels, length);
    if (! reader->read (&decoded, 0, length, 0, true, channels > 1))
    {
        error = "Can't read this file";
        return nullptr;
    }

    return std::make_shared<const ReferenceTrack> (file, resample (toStereo (std::move (decoded)), reader->sampleRate, sampleRate), sampleRate);
}

ReferenceTrack::ReferenceTrack (juce::File source, juce::AudioBuffer<float> stereoAudio, double sampleRate)
    : file (std::move (source)), audio (toStereo (std::move (stereoAudio))), rate (sampleRate)
{
    overview.assign (overviewBuckets, 0.0f);
    const auto length = audio.getNumSamples();
    for (int bucket = 0; bucket < overviewBuckets; ++bucket)
    {
        const auto start = static_cast<int> (static_cast<juce::int64> (length) * bucket / overviewBuckets);
        const auto end = static_cast<int> (static_cast<juce::int64> (length) * (bucket + 1) / overviewBuckets);
        if (end > start)
            overview[static_cast<size_t> (bucket)] = std::max (audio.getMagnitude (0, start, end - start), audio.getMagnitude (1, start, end - start));
    }

    slices = LoudnessMeter::sliceEnergies (audio, rate);
}

float ReferenceTrack::getLoudness (const LoopRegion& loop) const
{
    const auto range = loop.resolve (getLength(), rate);
    const auto sliceLength = static_cast<double> (juce::roundToInt (rate * 0.1));
    const auto sliceCount = static_cast<int> (slices.size());
    const auto first = std::min (sliceCount, static_cast<int> (std::round (static_cast<double> (range.getStart()) / sliceLength)));
    const auto last = std::min (sliceCount, static_cast<int> (std::round (static_cast<double> (range.getEnd()) / sliceLength)));
    return LoudnessMeter::integratedFromSlices (slices.data() + first, std::max (0, last - first));
}

LoopRegion ReferenceTrack::findLoudestSection (double seconds) const
{
    const auto window = static_cast<size_t> (std::round (seconds * 10.0));
    if (window == 0 || slices.size() <= window)
        return {};

    double sum = 0.0;
    for (size_t i = 0; i < window; ++i)
        sum += slices[i];

    auto loudest = sum;
    size_t loudestStart = 0;
    for (size_t start = 1; start + window <= slices.size(); ++start)
    {
        sum += slices[start + window - 1] - slices[start - 1];
        if (sum > loudest)
        {
            loudest = sum;
            loudestStart = start;
        }
    }

    const auto startSeconds = static_cast<double> (loudestStart) * 0.1;
    return { startSeconds, startSeconds + seconds };
}

} // namespace lucky
