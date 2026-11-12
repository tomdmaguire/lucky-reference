#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <CoreFoundation/CoreFoundation.h>
#include <cstdlib>
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

/** The snapshot's own library and state folders, so a snapshot never touches the real library. */
juce::File snapshotFolder (const juce::String& name)
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuckyReferenceSnapshot").getChildFile (name);
}

/** Writes a quiet FLAC of the given length, so library rows show realistic durations without large files. */
void writeQuietFlac (const juce::File& file, double seconds)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FlacAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    const auto options = juce::AudioFormatWriterOptions().withSampleRate (44100.0).withNumChannels (2).withBitsPerSample (16);
    if (auto writer = format.createWriterFor (stream, options))
    {
        juce::AudioBuffer<float> block (2, 44100);
        block.clear();
        for (auto written = 0.0; written < seconds; written += 1.0)
            writer->writeFromAudioSampleBuffer (block, 0, block.getNumSamples());
    }
}

/** A made-up song filtered to a tonal shape: lowpassed (dull), highpassed (thin) or squashed (dense), 30 s long. */
juce::AudioBuffer<float> shapedSong (int seed, double lowpassHz, double highpassHz, bool squash)
{
    auto song = fakeSong ({ -6, -3, -2, -4, -2 }, 6.0, seed);
    for (int channel = 0; channel < song.getNumChannels(); ++channel)
    {
        auto* samples = song.getWritePointer (channel);
        auto low = 0.0f, previous = 0.0f;
        const auto lowCoefficient = lowpassHz > 0.0 ? static_cast<float> (std::exp (-juce::MathConstants<double>::twoPi * lowpassHz / rate)) : 0.0f;
        const auto highCoefficient = highpassHz > 0.0 ? static_cast<float> (std::exp (-juce::MathConstants<double>::twoPi * highpassHz / rate)) : 0.0f;
        for (int i = 0; i < song.getNumSamples(); ++i)
        {
            auto sample = samples[i];
            if (lowpassHz > 0.0)
                sample = low = sample * (1.0f - lowCoefficient) + low * lowCoefficient;
            if (highpassHz > 0.0)
            {
                previous = sample * (1.0f - highCoefficient) + previous * highCoefficient;
                sample -= previous;
            }
            samples[i] = squash ? std::tanh (sample * 8.0f) * 0.25f : sample;
        }
    }
    return song;
}

/** Fills the library with 30-second songs of different tonal shapes, for ranking against the snapshot's mix. */
void fillShapedLibrary (const juce::File& library)
{
    struct Shape { const char* name; int seed; double lowpass, highpass; bool squash; };
    const Shape shapes[] { { "Club/Low Tide (Extended)", 21, 900.0, 0.0, false }, { "Club/Satellite Hearts", 22, 0.0, 0.0, false },
                           { "Club/Copper Sun", 23, 0.0, 0.0, true }, { "Indie/Paper Planes at Dawn", 24, 0.0, 400.0, false },
                           { "Indie/Harbour Lights", 25, 3000.0, 0.0, false }, { "Concrete Garden", 26, 0.0, 1500.0, true },
                           { "Velvet Static", 27, 0.0, 150.0, false } };
    for (const auto& shape : shapes)
    {
        const auto file = library.getChildFile (juce::String (shape.name) + ".flac");
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        const auto song = shapedSong (shape.seed, shape.lowpass, shape.highpass, shape.squash);
        juce::FlacAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        const auto options = juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels (2).withBitsPerSample (16);
        if (auto writer = format.createWriterFor (stream, options))
            writer->writeFromAudioSampleBuffer (song, 0, song.getNumSamples());
    }
}

/** Fills the library with made-up tracks in a couple of folders, plus an added folder outside it. */
void fillLibrary (const juce::File& library, const juce::File& state)
{
    const auto added = snapshotFolder ("Album Refs");
    writeQuietFlac (added.getChildFile ("Glasshouse Choir.flac"), 244.0);
    state.createDirectory();
    state.getChildFile ("folders.json").replaceWithText (juce::JSON::toString (juce::Array<juce::var> { added.getFullPathName() }));

    const std::pair<const char*, double> tracks[] { { "Club/Low Tide (Extended)", 412.0 }, { "Club/Satellite Hearts", 236.0 },
                                                    { "Club/Copper Sun", 301.0 }, { "Indie/Paper Planes at Dawn", 198.0 },
                                                    { "Indie/Harbour Lights", 254.0 }, { "Concrete Garden", 221.0 },
                                                    { "Velvet Static", 263.0 } };
    for (const auto& [path, seconds] : tracks)
        writeQuietFlac (library.getChildFile (juce::String (path) + ".flac"), seconds);
}

void setParameter (LuckyReferenceProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getParameters().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

/** Plays the mix through the processor in real time while the UI runs, so the radar reacts as it does in a host. */
void playFor (LuckyReferenceProcessor& processor, PlayingHead& head, const juce::AudioBuffer<float>& mix, double seconds)
{
    constexpr double frame = 1.0 / 60.0;
    const auto blocksPerFrame = rate * frame / blockSize;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> block (2, blockSize);
    auto owed = 0.0;
    for (auto elapsed = 0.0; elapsed < seconds; elapsed += frame)
    {
        for (owed += blocksPerFrame; owed >= 1.0; owed -= 1.0)
        {
            const auto start = static_cast<int> (head.time % (mix.getNumSamples() - blockSize));
            for (int channel = 0; channel < 2; ++channel)
                block.copyFrom (channel, 0, mix, channel, start, blockSize);
            processor.processBlock (block, midi);
            head.time += blockSize;
        }
        pumpMessages (frame);
    }
}

/** Renders the editor to a PNG. Modes: "empty", "loaded", "reference", "measuring", "nomatch", "radar" (flips sources to
    leave ripples), "playing" (flips to reference 2 while music plays; settleSeconds sets how long after the flip to capture),
    "library" (the library panel over the slots), "library-empty" (the panel with nothing in the library), "closest" (the panel ranked against a played mix), "closest-waiting" (the panel before enough of the mix has played) and "outside" (references from outside the library, offering ADD TO LIBRARY). */
int snapshot (const juce::File& output, const juce::String& mode, double settleSeconds)
{
    std::unique_ptr<LuckyReferenceProcessor> processor (dynamic_cast<LuckyReferenceProcessor*> (createPluginFilter()));
    processor->setPlayConfigDetails (2, 2, rate, blockSize);
    processor->prepareToPlay (rate, blockSize);
    PlayingHead head;
    processor->setPlayHead (&head);

    if (mode == "outside")
        juce::SharedResourcePointer<lucky::Settings>()->setOpenedLibrary();

    if (mode != "empty" && mode != "library-empty")
    {
        const auto folder = mode == "outside" ? snapshotFolder ("Elsewhere") : snapshotFolder ("Library");
        folder.createDirectory();
        const juce::File files[] { folder.getChildFile ("Midnight Drive - Master.wav"), folder.getChildFile ("Glass Houses (Club Mix).wav"),
                                   folder.getChildFile ("Northern Lights.wav") };
        writeWav (files[0], fakeSong ({ -20, -8, -4, -10, -2, -2, -14 }, 24.0, 1));
        writeWav (files[1], fakeSong ({ -14, -6, 0, 0, -16, -2, 1, 1, -20 }, 20.0, 2));
        writeWav (files[2], fakeSong ({ -10, -4, -4, -12, -3, -3, -6 }, 26.0, 3));
        const auto slotsToLoad = mode.startsWith ("closest") ? 1 : 3;
        for (int i = 0; i < slotsToLoad; ++i)
            processor->loadFile (i, files[i]);

        for (int attempt = 0; attempt < 300; ++attempt)
        {
            auto loaded = true;
            for (int i = 0; i < slotsToLoad; ++i)
                loaded = loaded && processor->getTrack (i) != nullptr;
            if (loaded)
                break;
            pumpMessages (0.02);
        }

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
        const auto mix = fakeSong ({ -3 }, 6.0, 4);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> work (mix);
        for (int start = 0; start + blockSize <= work.getNumSamples(); start += blockSize)
        {
            juce::AudioBuffer<float> block (work.getArrayOfWritePointers(), 2, start, blockSize);
            processor->processBlock (block, midi);
            head.time += blockSize;
        }
    }

    if (mode == "closest")
    {
        const auto mix = fakeSong ({ -3, -2, -4 }, 5.0, 4);
        for (int pass = 0; pass < 2; ++pass)
        {
            juce::MidiBuffer midi;
            for (int start = 0; start + blockSize <= mix.getNumSamples(); start += blockSize)
            {
                juce::AudioBuffer<float> block (2, blockSize);
                for (int channel = 0; channel < 2; ++channel)
                    block.copyFrom (channel, 0, mix, channel, start, blockSize);
                processor->processBlock (block, midi);
                head.time += blockSize;
                if (start % (blockSize * 200) == 0)
                    processor->updateMixProfile();
            }
            processor->updateMixProfile();
        }
        processor->getLibrary().rescan();
        for (int attempt = 0; attempt < 500 && processor->getLibrary().isReading(); ++attempt)
            pumpMessages (0.02);
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
    if (mode.startsWith ("library") || mode.startsWith ("closest"))
    {
        dynamic_cast<LuckyReferenceEditor&> (*editor).showLibrary (mode.startsWith ("closest"));
        for (int attempt = 0; attempt < 300 && processor->getLibrary().isReading(); ++attempt)
            pumpMessages (0.02);
        pumpMessages (0.3);
        if (mode == "closest")
        {
            playFor (*processor, head, shapedSong (30, 900.0, 0.0, false), 90.0);
            processor->updateMixProfile();
            pumpMessages (0.6);
        }
    }

    if (mode == "playing")
    {
        const auto mix = fakeSong ({ -3 }, 6.0, 4);
        playFor (*processor, head, mix, 3.0);
        setParameter (*processor, LuckyReferenceProcessor::slotId, 1.0f);
        setParameter (*processor, LuckyReferenceProcessor::referenceId, 1.0f);
        playFor (*processor, head, mix, settleSeconds);
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
    if (argc < 3)
    {
        std::cerr << "usage: Snapshot <output.png> <empty|loaded|reference|measuring|nomatch|radar|playing|library|library-empty|closest|closest-waiting|outside> [settleSeconds]\n";
        return 1;
    }

    const auto library = snapshotFolder ("Library"), state = snapshotFolder ("State");
    snapshotFolder ({}).deleteRecursively();
    library.createDirectory();
    if (juce::String (argv[2]) == "library")
        fillLibrary (library, state);
    if (juce::String (argv[2]).startsWith ("closest"))
        fillShapedLibrary (library);
    setenv ("LUCKY_REFERENCE_LIBRARY", library.getFullPathName().toRawUTF8(), 1);
    setenv ("LUCKY_REFERENCE_STATE", state.getFullPathName().toRawUTF8(), 1);

    juce::ScopedJuceInitialiser_GUI juce;
    return snapshot (juce::File (juce::String (argv[1])), juce::String (argv[2]), argc > 3 ? std::atof (argv[3]) : 0.6);
}
