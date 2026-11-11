#include "PluginProcessor.h"
#include "TestSignals.h"
#include <CoreFoundation/CoreFoundation.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double rate = 48000.0;
constexpr int blockSize = 512;

/** A host transport that is always playing and advances by whatever is processed. */
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

std::unique_ptr<LuckyReferenceProcessor> makeProcessor()
{
    std::unique_ptr<LuckyReferenceProcessor> processor (dynamic_cast<LuckyReferenceProcessor*> (createPluginFilter()));
    processor->setPlayConfigDetails (2, 2, rate, blockSize);
    processor->prepareToPlay (rate, blockSize);
    return processor;
}

/** Lets queued messages (loader results) run until the condition holds or a few seconds pass. */
bool pumpUntil (const std::function<bool()>& condition)
{
    for (int attempt = 0; attempt < 500 && ! condition(); ++attempt)
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.01, false);
    return condition();
}

void setParameter (LuckyReferenceProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getParameters().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

/** Processes the mix in blocks through the processor and returns the output. */
juce::AudioBuffer<float> run (LuckyReferenceProcessor& processor, PlayingHead& head, const juce::AudioBuffer<float>& mix)
{
    juce::AudioBuffer<float> output (mix);
    juce::MidiBuffer midi;
    for (int start = 0; start < output.getNumSamples(); start += blockSize)
    {
        juce::AudioBuffer<float> block (output.getArrayOfWritePointers(), 2, start, std::min (blockSize, output.getNumSamples() - start));
        processor.processBlock (block, midi);
        head.time += block.getNumSamples();
    }
    return output;
}

bool identical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < 2; ++ch)
        if (std::memcmp (a.getReadPointer (ch), b.getReadPointer (ch), sizeof (float) * static_cast<size_t> (a.getNumSamples())) != 0)
            return false;
    return true;
}
} // namespace

class ProcessorTests final : public juce::UnitTest
{
public:
    ProcessorTests() : juce::UnitTest ("Processor", "Lucky Reference Processor") {}

    void runTest() override
    {
        const auto folder = lucky::test::scratchFolder ("Processor");
        const auto referenceFile = folder.getChildFile ("reference.wav");
        lucky::test::writeWav (referenceFile, lucky::test::sine (-14.0f, 10.0, rate, 2, 500.0), rate);
        const auto mix = lucky::test::sine (-20.0f, 1.0, rate);

        beginTest ("Parameters: the A/B switch comes first, then slot and match");
        {
            auto processor = makeProcessor();
            const auto& parameters = processor->getParameters().processor.getParameters();
            expectEquals (parameters.size(), 3);
            expectEquals (dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[0])->paramID, LuckyReferenceProcessor::referenceId);
            expectEquals (parameters[0]->getCurrentValueAsText(), juce::String ("Mix"));
            expectEquals (processor->getParameters().getRawParameterValue (LuckyReferenceProcessor::matchId)->load(), 1.0f);
        }

        beginTest ("Turning the reference on from the host switches the output with no editor open");
        {
            auto processor = makeProcessor();
            PlayingHead head;
            processor->setPlayHead (&head);
            processor->loadFile (0, referenceFile);
            expect (pumpUntil ([&] { return processor->getTrack (0) != nullptr; }));

            expect (identical (run (*processor, head, mix), mix));
            setParameter (*processor, LuckyReferenceProcessor::referenceId, 1.0f);
            const auto output = run (*processor, head, mix);
            expect (! identical (output, mix));
            expect (output.getMagnitude (0, output.getNumSamples() - blockSize * 4, blockSize) > 0.05f);
        }

        beginTest ("Exports always contain the mix");
        {
            auto processor = makeProcessor();
            PlayingHead head;
            processor->setPlayHead (&head);
            processor->loadFile (0, referenceFile);
            expect (pumpUntil ([&] { return processor->getTrack (0) != nullptr; }));
            setParameter (*processor, LuckyReferenceProcessor::referenceId, 1.0f);
            processor->setNonRealtime (true);
            expect (identical (run (*processor, head, mix), mix));
        }

        beginTest ("A newly loaded reference loops its loudest section");
        {
            const auto songFile = folder.getChildFile ("song.wav");
            lucky::test::writeWav (songFile,
                                   lucky::test::concatenate (lucky::test::sine (-30.0f, 30.0, rate), lucky::test::sine (-12.0f, 20.0, rate)), rate);
            auto processor = makeProcessor();
            processor->loadFile (0, songFile);
            expect (pumpUntil ([&] { return processor->getTrack (0) != nullptr; }));
            const auto loop = processor->getLoop (0);
            expect (loop.isSet());
            expect (loop.startSeconds >= 29.9 && loop.endSeconds <= 50.0, juce::String (loop.startSeconds) + " to " + juce::String (loop.endSeconds));
            expectWithinAbsoluteError (loop.endSeconds - loop.startSeconds, LuckyReferenceProcessor::autoLoopSeconds, 1.0e-9);
        }

        beginTest ("Sessions restore files, loops and parameters");
        {
            auto original = makeProcessor();
            original->loadFile (2, referenceFile);
            expect (pumpUntil ([&] { return original->getTrack (2) != nullptr; }));
            original->setLoop (2, { 2.5, 7.0 });
            setParameter (*original, LuckyReferenceProcessor::referenceId, 1.0f);
            setParameter (*original, LuckyReferenceProcessor::slotId, 2.0f);
            setParameter (*original, LuckyReferenceProcessor::matchId, 0.0f);

            juce::MemoryBlock state;
            original->getStateInformation (state);

            auto restored = makeProcessor();
            restored->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
            expect (pumpUntil ([&] { return restored->getTrack (2) != nullptr; }));
            expectEquals (restored->getFile (2).getFullPathName(), referenceFile.getFullPathName());
            expectWithinAbsoluteError (restored->getLoop (2).startSeconds, 2.5, 1.0e-9);
            expectWithinAbsoluteError (restored->getLoop (2).endSeconds, 7.0, 1.0e-9);
            expect (restored->getFile (0) == juce::File());
            const auto& parameters = restored->getParameters();
            expectEquals (parameters.getRawParameterValue (LuckyReferenceProcessor::referenceId)->load(), 1.0f);
            expectEquals (parameters.getRawParameterValue (LuckyReferenceProcessor::slotId)->load(), 2.0f);
            expectEquals (parameters.getRawParameterValue (LuckyReferenceProcessor::matchId)->load(), 0.0f);
        }

        beginTest ("A missing file keeps its path and reports the problem");
        {
            auto processor = makeProcessor();
            const auto missing = folder.getChildFile ("moved.wav");
            processor->loadFile (1, missing);
            expect (pumpUntil ([&] { return processor->getError (1).isNotEmpty(); }));
            expectEquals (processor->getFile (1).getFullPathName(), missing.getFullPathName());
            expect (processor->getTrack (1) == nullptr);
        }
    }
};

static ProcessorTests processorTests;

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("Lucky Reference Processor");

    for (int i = 0; i < runner.getNumResults(); ++i)
        if (runner.getResult (i)->failures > 0)
            return 1;
    return 0;
}
