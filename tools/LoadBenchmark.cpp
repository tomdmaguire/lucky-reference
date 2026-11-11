#include "dsp/LoudnessMeter.h"
#include "ref/ReferenceTrack.h"
#include <iostream>

namespace
{
double secondsSince (juce::int64 startTicks)
{
    return juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - startTicks);
}
} // namespace

/** Writes four minutes of 44.1 kHz stereo noise to a temporary WAV, standing in for a typical song. */
static juce::File writeSyntheticSong()
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuckyReferenceBenchmarkSong.wav");
    juce::AudioBuffer<float> song (2, 44100 * 240);
    juce::Random random (7);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < song.getNumSamples(); ++i)
            song.setSample (ch, i, random.nextFloat() * 0.5f - 0.25f);

    file.deleteFile();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    const auto options = juce::AudioFormatWriterOptions().withSampleRate (44100.0).withNumChannels (2).withBitsPerSample (24);
    if (auto writer = format.createWriterFor (stream, options))
        writer->writeFromAudioSampleBuffer (song, 0, song.getNumSamples());
    return file;
}

/** Times decoding, analysis and a whole ReferenceTrack::load for a file (or "--synthetic" for a 4-minute 44.1 kHz song) at a host rate. */
int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: LoadBenchmark <audio file | --synthetic> [host rate]\n";
        return 1;
    }

    const auto argument = juce::String (juce::CharPointer_UTF8 (argv[1]));
    const auto file = argument == "--synthetic" ? writeSyntheticSong() : juce::File (argument);
    const auto rate = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 48000.0;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    auto start = juce::Time::getHighResolutionTicks();
    const std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        std::cerr << "can't read " << file.getFullPathName() << "\n";
        return 1;
    }
    juce::AudioBuffer<float> decoded (2, static_cast<int> (reader->lengthInSamples));
    reader->read (&decoded, 0, decoded.getNumSamples(), 0, true, true);
    std::cout << "decode:   " << secondsSince (start) << " s for " << decoded.getNumSamples() / reader->sampleRate << " s at "
              << reader->sampleRate << " Hz (" << reader->getFormatName() << ")\n";

    start = juce::Time::getHighResolutionTicks();
    const auto slices = lucky::LoudnessMeter::sliceEnergies (decoded, reader->sampleRate);
    std::cout << "slices:   " << secondsSince (start) << " s\n";

    start = juce::Time::getHighResolutionTicks();
    juce::String error;
    const auto track = lucky::ReferenceTrack::load (file, rate, formats, error);
    std::cout << "full load at " << rate << " Hz: " << secondsSince (start) << " s" << (track ? "" : " FAILED: " + error) << "\n";
    return 0;
}
