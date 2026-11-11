#include "dsp/LoudnessMeter.h"

namespace
{
struct Section
{
    float dbfs;
    double seconds;
};

/** Stereo 1 kHz sine sequence with each section at the given peak level, like the EBU Tech 3341 test signals. */
juce::AudioBuffer<float> sineSequence (std::initializer_list<Section> sections, double sampleRate)
{
    double totalSeconds = 0.0;
    for (const auto& section : sections)
        totalSeconds += section.seconds;

    juce::AudioBuffer<float> buffer (2, static_cast<int> (std::round (totalSeconds * sampleRate)));
    const auto step = juce::MathConstants<double>::twoPi * 1000.0 / sampleRate;
    int position = 0;

    for (const auto& section : sections)
    {
        const auto amplitude = juce::Decibels::decibelsToGain (section.dbfs, -200.0f);
        const auto end = std::min (buffer.getNumSamples(), position + static_cast<int> (std::round (section.seconds * sampleRate)));
        for (; position < end; ++position)
        {
            const auto sample = amplitude * static_cast<float> (std::sin (step * position));
            buffer.setSample (0, position, sample);
            buffer.setSample (1, position, sample);
        }
    }

    return buffer;
}
} // namespace

class LoudnessMeterTests final : public juce::UnitTest
{
public:
    LoudnessMeterTests() : juce::UnitTest ("Loudness meter", "Lucky Reference") {}

    void runTest() override
    {
        for (const auto rate : { 44100.0, 48000.0, 96000.0 })
        {
            beginTest ("EBU Tech 3341 integrated cases at " + juce::String (rate) + " Hz");
            expectLufs (sineSequence ({ { -23.0f, 20.0 } }, rate), rate, -23.0f, "case 1");
            expectLufs (sineSequence ({ { -33.0f, 20.0 } }, rate), rate, -33.0f, "case 2");
            expectLufs (sineSequence ({ { -36.0f, 10.0 }, { -23.0f, 60.0 }, { -36.0f, 10.0 } }, rate), rate, -23.0f, "case 3");
            expectLufs (sineSequence ({ { -72.0f, 10.0 }, { -36.0f, 10.0 }, { -23.0f, 60.0 }, { -36.0f, 10.0 }, { -72.0f, 10.0 } }, rate),
                        rate, -23.0f, "case 4");
            expectLufs (sineSequence ({ { -26.0f, 20.0 }, { -20.0f, 20.1 }, { -26.0f, 20.0 } }, rate), rate, -23.0f, "case 5");
        }

        beginTest ("Silence has no loudness");
        {
            juce::AudioBuffer<float> silence (2, 48000 * 5);
            silence.clear();
            expect (std::isinf (lucky::LoudnessMeter::measure (silence, 0, silence.getNumSamples(), 48000.0)));
        }

        beginTest ("Streaming in small blocks matches one pass, and reset starts over");
        {
            const auto signal = sineSequence ({ { -30.0f, 6.0 } }, 48000.0);
            lucky::LoudnessMeter meter;
            meter.prepare (48000.0);

            for (int start = 0; start < signal.getNumSamples(); start += 37)
            {
                const auto n = std::min (37, signal.getNumSamples() - start);
                const float* channels[] { signal.getReadPointer (0, start), signal.getReadPointer (1, start) };
                meter.process (channels, 2, n);
            }

            expectWithinAbsoluteError (meter.getIntegrated(), -30.0f, 0.1f);
            expectWithinAbsoluteError (meter.getGatedSeconds(), 5.7, 0.01);

            meter.reset();
            expect (std::isinf (meter.getIntegrated()));
            expectEquals (meter.getGatedSeconds(), 0.0);
        }
    }

private:
    void expectLufs (const juce::AudioBuffer<float>& signal, double rate, float expected, const juce::String& name)
    {
        const auto measured = lucky::LoudnessMeter::measure (signal, 0, signal.getNumSamples(), rate);
        expectWithinAbsoluteError (measured, expected, 0.1f, name + " measured " + juce::String (measured, 2));
    }
};

static LoudnessMeterTests loudnessMeterTests;
