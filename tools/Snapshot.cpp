#include "PluginProcessor.h"
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double rate = 48000.0;
constexpr int blockSize = 512;

void pumpMessages (double seconds)
{
    CFRunLoopRunInMode (kCFRunLoopDefaultMode, seconds, false);
}

/** A playing host transport that advances by whatever is processed. */
struct PlayingHead final : juce::AudioPlayHead
{
    juce::int64 time = 0;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying (true);
        info.setTimeInSamples (time);
        info.setBpm (120.0);
        info.setPpqPosition (static_cast<double> (time) / rate * 2.0);
        return info;
    }
};

/** Noise with a kick-like pulse every beat, shaped by a list of section levels, so the waveform looks like a song. */
juce::AudioBuffer<float> fakeSong (std::initializer_list<float> sectionDb, double sectionSeconds, int seed)
{
    juce::Random random (seed);
    const auto sectionLength = static_cast<int> (sectionSeconds * rate);
    juce::AudioBuffer<float> song (2, sectionLength * static_cast<int> (sectionDb.size()));
    int position = 0;
    float smoothed = 0.0f;
    for (const auto db : sectionDb)
    {
        const auto gain = juce::Decibels::decibelsToGain (db);
        for (int i = 0; i < sectionLength; ++i, ++position)
        {
            const auto beat = std::fmod (position / rate, 0.5) / 0.5;
            const auto pulse = static_cast<float> (std::exp (-beat * 8.0));
            smoothed += 0.3f * (random.nextFloat() * 2.0f - 1.0f - smoothed);
            const auto sample = gain * (0.35f * smoothed + 0.6f * pulse * static_cast<float> (std::sin (position * 0.006)));
            song.setSample (0, position, sample);
            song.setSample (1, position, sample * 0.95f);
        }
    }
    return song;
}

void writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer)
{
    file.deleteFile();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    const auto options = juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels (2).withBitsPerSample (24);
    if (auto writer = format.createWriterFor (stream, options))
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

void setParameter (LuckyReferenceProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getParameters().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

/** Renders the editor to a PNG. Modes: "empty", "loaded", "reference", "measuring", "nomatch", "radar" (flips sources to leave ripples). */
int snapshot (const juce::File& output, const juce::String& mode)
{
    std::unique_ptr<LuckyReferenceProcessor> processor (dynamic_cast<LuckyReferenceProcessor*> (createPluginFilter()));
    processor->setPlayConfigDetails (2, 2, rate, blockSize);
    processor->prepareToPlay (rate, blockSize);
    PlayingHead head;
    processor->setPlayHead (&head);

    if (mode != "empty")
    {
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuckyReferenceSnapshot");
        folder.createDirectory();
        const juce::File files[] { folder.getChildFile ("Midnight Drive - Master.wav"), folder.getChildFile ("Glass Houses (Club Mix).wav"),
                                   folder.getChildFile ("Northern Lights.wav") };
        writeWav (files[0], fakeSong ({ -30, -18, -14, -20, -12, -12, -24 }, 24.0, 1));
        writeWav (files[1], fakeSong ({ -24, -16, -10, -10, -26, -12, -9, -9, -30 }, 20.0, 2));
        writeWav (files[2], fakeSong ({ -20, -14, -14, -22, -13, -13, -16 }, 26.0, 3));
        for (int i = 0; i < 3; ++i)
            processor->loadFile (i, files[i]);

        for (int attempt = 0; attempt < 300 && (processor->getTrack (0) == nullptr || processor->getTrack (1) == nullptr || processor->getTrack (2) == nullptr); ++attempt)
            pumpMessages (0.02);

        processor->setLoop (0, { 50.0, 72.0 });
        processor->setLoop (1, { 120.0, 140.0 });
    }

    if (mode == "reference" || mode == "nomatch")
    {
        setParameter (*processor, LuckyReferenceProcessor::slotId, 1.0f);
        setParameter (*processor, LuckyReferenceProcessor::referenceId, 1.0f);
    }
    if (mode == "nomatch")
        setParameter (*processor, LuckyReferenceProcessor::matchId, 0.0f);

    if (mode != "measuring" && mode != "empty")
    {
        const auto mix = fakeSong ({ -19 }, 6.0, 4);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> work (mix);
        for (int start = 0; start + blockSize <= work.getNumSamples(); start += blockSize)
        {
            juce::AudioBuffer<float> block (work.getArrayOfWritePointers(), 2, start, blockSize);
            processor->processBlock (block, midi);
            head.time += blockSize;
        }
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());
    pumpMessages (1.0);

    if (mode == "radar")
    {
        for (const auto& [reference, slot] : { std::pair { 1.0f, 0.0f }, std::pair { 1.0f, 1.0f }, std::pair { 0.0f, 1.0f }, std::pair { 0.0f, 2.0f } })
        {
            setParameter (*processor, LuckyReferenceProcessor::slotId, slot);
            setParameter (*processor, LuckyReferenceProcessor::referenceId, reference);
            pumpMessages (0.35);
        }
    }
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    output.deleteFile();
    juce::FileOutputStream stream (output);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    processor->editorBeingDeleted (editor.get());
    std::cout << "wrote " << output.getFullPathName() << "\n";
    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    if (argc < 3)
    {
        std::cerr << "usage: Snapshot <output.png> <empty|loaded|reference|measuring|nomatch|radar>\n";
        return 1;
    }
    return snapshot (juce::File (juce::String (argv[1])), juce::String (argv[2]));
}
