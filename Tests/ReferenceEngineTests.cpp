#include "TestSignals.h"
#include "dsp/ReferenceEngine.h"

using namespace lucky;

namespace
{
constexpr double rate = 48000.0;
constexpr int blockSize = 512;

/** Drives an engine block by block with a rolling host transport, collecting the output. */
struct Harness
{
    ReferenceEngine engine;
    ReferenceEngine::Controls controls;
    ReferenceEngine::Transport transport;

    Harness()
    {
        engine.prepare (rate, blockSize);
        transport.isPlaying = true;
        transport.timeInSamples = 0;
        transport.bpm = 120.0;
        transport.ppqPosition = 0.0;
    }

    /** Runs the mix through the engine and returns what it outputs. */
    juce::AudioBuffer<float> run (const juce::AudioBuffer<float>& mix)
    {
        juce::AudioBuffer<float> output (mix);
        for (int start = 0; start < output.getNumSamples(); start += blockSize)
        {
            const auto n = std::min (blockSize, output.getNumSamples() - start);
            juce::AudioBuffer<float> block (output.getArrayOfWritePointers(), 2, start, n);
            engine.process (block, controls, transport);
            advance (n);
        }
        return output;
    }

    void advance (int n)
    {
        if (! transport.isPlaying)
            return;
        *transport.timeInSamples += n;
        *transport.ppqPosition += n / rate * *transport.bpm / 60.0;
    }

    void jumpTo (double seconds)
    {
        transport.timeInSamples = static_cast<juce::int64> (seconds * rate);
        transport.ppqPosition = seconds * *transport.bpm / 60.0;
    }
};

ReferenceTrackPtr track (const juce::AudioBuffer<float>& audio)
{
    return std::make_shared<const ReferenceTrack> (juce::File(), audio, rate);
}

float loudnessOfEnd (const juce::AudioBuffer<float>& buffer, double seconds)
{
    const auto length = static_cast<int> (seconds * rate);
    return LoudnessMeter::measure (buffer, buffer.getNumSamples() - length, length, rate);
}

float largestStep (const juce::AudioBuffer<float>& buffer, int from = 1)
{
    float largest = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = std::max (1, from); i < buffer.getNumSamples(); ++i)
            largest = std::max (largest, std::abs (buffer.getSample (ch, i) - buffer.getSample (ch, i - 1)));
    return largest;
}
} // namespace

class ReferenceEngineTests final : public juce::UnitTest
{
public:
    ReferenceEngineTests() : juce::UnitTest ("Reference engine", "Lucky Reference") {}

    void runTest() override
    {
        const auto mix = test::sine (-20.0f, 8.0, rate);
        const auto loudReference = track (test::sine (-14.0f, 20.0, rate, 2, 1000.0));

        beginTest ("The mix passes through bit for bit while the switch is on the mix");
        {
            Harness harness;
            harness.engine.setSlot (0, loudReference, {});
            const auto output = harness.run (mix);
            auto identical = true;
            for (int ch = 0; ch < 2; ++ch)
                identical = identical && std::memcmp (output.getReadPointer (ch), mix.getReadPointer (ch), sizeof (float) * static_cast<size_t> (mix.getNumSamples())) == 0;
            expect (identical);
        }

        beginTest ("A reference is turned to the mix's loudness");
        {
            Harness harness;
            harness.engine.setSlot (1, loudReference, {});
            harness.controls = { true, 1, true };
            const auto output = harness.run (mix);
            expectWithinAbsoluteError (harness.engine.getMixLoudness(), -20.0f, 0.1f);
            expectWithinAbsoluteError (harness.engine.getReferenceLoudness (1), -14.0f, 0.1f);
            expectWithinAbsoluteError (loudnessOfEnd (output, 2.0), -20.0f, 0.2f);
        }

        beginTest ("With Match off the reference plays at its own level");
        {
            Harness harness;
            harness.engine.setSlot (1, loudReference, {});
            harness.controls = { true, 1, false };
            const auto output = harness.run (mix);
            expectWithinAbsoluteError (loudnessOfEnd (output, 2.0), -14.0f, 0.2f);
        }

        beginTest ("Blocks larger than announced are processed in full");
        {
            Harness harness;
            harness.engine.setSlot (0, loudReference, {});
            harness.controls = { true, 0, false };
            juce::AudioBuffer<float> big (2, blockSize * 5);
            big.clear();
            for (int pass = 0; pass < 4; ++pass)
            {
                harness.engine.process (big, harness.controls, harness.transport);
                harness.advance (big.getNumSamples());
            }
            expect (big.getMagnitude (0, big.getNumSamples() - blockSize, blockSize) > 0.1f);
            expectEquals (harness.engine.getPosition (0), juce::int64 { blockSize * 20 });
        }

        beginTest ("Before the mix has been measured, the reference is not turned");
        {
            Harness harness;
            harness.engine.setSlot (0, loudReference, {});
            harness.controls = { true, 0, true };
            const auto output = harness.run (test::sine (-20.0f, 2.0, rate));
            expect (harness.engine.isMeasuring());
            expectWithinAbsoluteError (loudnessOfEnd (output, 1.5), -14.0f, 0.2f);
        }

        beginTest ("Starting the transport restarts every reference at its loop start");
        {
            Harness harness;
            harness.engine.setSlot (0, loudReference, { 5.0, 9.0 });
            harness.engine.setSlot (2, loudReference, { 12.0, 15.0 });
            harness.run (test::sine (-20.0f, 1.0, rate));
            expect (harness.engine.getPosition (0) > static_cast<juce::int64> (5.5 * rate));

            harness.transport.isPlaying = false;
            harness.run (test::sine (-20.0f, 0.1, rate));
            harness.transport.isPlaying = true;
            juce::AudioBuffer<float> block (2, blockSize);
            block.clear();
            harness.engine.process (block, harness.controls, harness.transport);
            expectEquals (harness.engine.getPosition (0), static_cast<juce::int64> (5.0 * rate) + blockSize);
            expectEquals (harness.engine.getPosition (2), static_cast<juce::int64> (12.0 * rate) + blockSize);
        }

        beginTest ("The loop wraps without a click");
        {
            Harness harness;
            harness.engine.setSlot (0, track (test::sine (-6.0f, 10.0, rate, 2, 1000.0)), { 1.0, 3.0137 });
            harness.controls = { true, 0, false };
            juce::AudioBuffer<float> silence (2, static_cast<int> (5.0 * rate));
            silence.clear();
            const auto output = harness.run (silence);
            const auto amplitude = juce::Decibels::decibelsToGain (-6.0f);
            const auto normalStep = amplitude * juce::MathConstants<float>::twoPi * 1000.0f / static_cast<float> (rate);
            expect (largestStep (output, blockSize * 4) < normalStep * 1.5f, "step " + juce::String (largestStep (output, blockSize * 4)));
        }

        beginTest ("Switching between the mix and a reference doesn't click");
        {
            Harness harness;
            harness.engine.setSlot (0, track (test::sine (-6.0f, 10.0, rate, 2, 1000.0)), {});
            const auto quietMix = test::sine (-6.0f, 0.5, rate, 2, 300.0);
            juce::AudioBuffer<float> output (2, 0);
            float largest = 0.0f;
            for (int flip = 0; flip < 6; ++flip)
            {
                harness.controls.reference = ! harness.controls.reference;
                largest = std::max (largest, largestStep (harness.run (quietMix)));
            }
            const auto amplitude = juce::Decibels::decibelsToGain (-6.0f);
            expect (largest < amplitude * 0.2f, "step " + juce::String (largest));
        }

        beginTest ("Jumping resets the mix measurement, a host loop wrap does not");
        {
            const auto quiet = test::sine (-30.0f, 5.0, rate), loud = test::sine (-20.0f, 5.0, rate);

            Harness jumped;
            jumped.run (quiet);
            jumped.jumpTo (60.0);
            jumped.run (loud);
            expectWithinAbsoluteError (jumped.engine.getMixLoudness(), -20.0f, 0.1f);

            Harness looped;
            looped.transport.loopPpq = std::pair (0.0, 10.0);
            looped.run (quiet);
            looped.jumpTo (0.0);
            looped.run (loud);
            expect (looped.engine.getMixLoudness() < -20.5f, "loudness " + juce::String (looped.engine.getMixLoudness()));
        }
    }
};

static ReferenceEngineTests referenceEngineTests;
