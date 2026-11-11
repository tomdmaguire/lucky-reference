#include "library/TonalProfile.h"
#include <juce_core/juce_core.h>

namespace lucky
{

namespace
{
constexpr double rate = 48000.0;

std::vector<float> noise (double seconds, int seed, float dbfs = -12.0f)
{
    juce::Random random (seed);
    const auto gain = juce::Decibels::decibelsToGain (dbfs);
    std::vector<float> samples (static_cast<size_t> (seconds * rate));
    for (auto& sample : samples)
        sample = gain * (random.nextFloat() * 2.0f - 1.0f);
    return samples;
}

std::vector<float> lowpassed (std::vector<float> samples, double hz)
{
    const auto coefficient = static_cast<float> (std::exp (-juce::MathConstants<double>::twoPi * hz / rate));
    auto state = 0.0f;
    for (auto& sample : samples)
        sample = state = sample * (1.0f - coefficient) + state * coefficient;
    return samples;
}

std::vector<float> highpassed (const std::vector<float>& samples, double hz)
{
    const auto low = lowpassed (samples, hz);
    std::vector<float> high (samples.size());
    for (size_t i = 0; i < samples.size(); ++i)
        high[i] = samples[i] - low[i];
    return high;
}

/** The samples scaled to a given RMS level, so differently filtered test signals are equally loud. */
std::vector<float> atLevel (std::vector<float> samples, float rmsDb)
{
    double sum = 0.0;
    for (const auto sample : samples)
        sum += sample * sample;
    const auto gain = juce::Decibels::decibelsToGain (rmsDb) / static_cast<float> (std::sqrt (sum / static_cast<double> (samples.size())) + 1.0e-12);
    for (auto& sample : samples)
        sample *= gain;
    return samples;
}

std::vector<float> scaled (std::vector<float> samples, float db)
{
    const auto gain = juce::Decibels::decibelsToGain (db);
    for (auto& sample : samples)
        sample *= gain;
    return samples;
}

std::vector<float> joined (std::initializer_list<std::vector<float>> parts)
{
    std::vector<float> all;
    for (const auto& part : parts)
        all.insert (all.end(), part.begin(), part.end());
    return all;
}

std::vector<float> silence (double seconds)
{
    return std::vector<float> (static_cast<size_t> (seconds * rate), 0.0f);
}

/** Noise hits every half second that decay quickly, like a drum loop. */
std::vector<float> drums (double seconds, int seed)
{
    auto samples = noise (seconds, seed, -3.0f);
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const auto beat = std::fmod (static_cast<double> (i) / rate, 0.5);
        samples[i] *= static_cast<float> (0.05 + std::exp (-beat * 30.0));
    }
    return samples;
}

/** Heavy saturation, which flattens the peaks the way a hard limiter does. */
std::vector<float> squashed (std::vector<float> samples)
{
    for (auto& sample : samples)
        sample = std::tanh (sample * 12.0f) * 0.3f;
    return samples;
}

TonalProfile profileOf (const std::vector<float>& samples, double windowSeconds = 0.0)
{
    TonalAnalyser analyser (rate, windowSeconds);
    analyser.process (samples.data(), static_cast<int> (samples.size()));
    return analyser.getProfile().value_or (TonalProfile {});
}

int band (double hz)
{
    for (int i = 0; i < TonalProfile::numBands; ++i)
        if (TonalProfile::bandCentreHz (i) >= hz * 0.95)
            return i;
    return TonalProfile::numBands - 1;
}
} // namespace

class TonalProfileTests final : public juce::UnitTest
{
public:
    TonalProfileTests() : juce::UnitTest ("Tonal profile", "Lucky Reference") {}

    void runTest() override
    {
        beginTest ("Bands run from 31.5 Hz to 12.5 kHz");
        expectWithinAbsoluteError (TonalProfile::bandCentreHz (0), 31.25, 0.01);
        expectWithinAbsoluteError (TonalProfile::bandCentreHz (TonalProfile::numBands - 1), 12699.2, 1.0);

        beginTest ("Loudness has no effect");
        {
            const auto music = drums (20.0, 1);
            expectLessThan (TonalProfile::distance (profileOf (music), profileOf (scaled (music, -20.0f))), 0.05f);
            expectWithinAbsoluteError (TonalProfile::match (profileOf (music), profileOf (music)), 1.0f, 1.0e-6f);
        }

        beginTest ("A dull track has less top end than a bright one");
        {
            const auto bright = profileOf (noise (20.0, 2));
            const auto dull = profileOf (lowpassed (noise (20.0, 2), 500.0));
            const auto top = static_cast<size_t> (band (8000.0)), bottom = static_cast<size_t> (band (100.0));
            expectGreaterThan ((dull.bandsDb[bottom] - dull.bandsDb[top]) - (bright.bandsDb[bottom] - bright.bandsDb[top]), 20.0f);
            expectGreaterThan (TonalProfile::distance (bright, dull), 6.0f);
            expectLessThan (TonalProfile::match (bright, dull), 0.3f);
        }

        beginTest ("Silence and quiet passages are ignored");
        {
            const auto music = drums (20.0, 3);
            const auto padded = joined ({ silence (10.0), scaled (drums (5.0, 4), -50.0f), music, silence (10.0) });
            expectLessThan (TonalProfile::distance (profileOf (music), profileOf (padded)), 0.3f);

            TonalAnalyser analyser (rate);
            const auto all = joined ({ music, silence (10.0) });
            analyser.process (all.data(), static_cast<int> (all.size()));
            expectWithinAbsoluteError (analyser.getGatedSeconds(), 20.0, 0.2);

            TonalAnalyser quiet (rate);
            const auto nothing = silence (5.0);
            quiet.process (nothing.data(), static_cast<int> (nothing.size()));
            expect (! quiet.getProfile().has_value());
        }

        beginTest ("Compression lowers punch");
        {
            const auto open = profileOf (drums (20.0, 5));
            const auto limited = profileOf (squashed (drums (20.0, 5)));
            expectGreaterThan (open.punchDb - limited.punchDb, 3.0f);
        }

        beginTest ("With a window, older audio is forgotten");
        {
            const auto later = atLevel (noise (40.0, 6), -18.0f);
            const auto both = joined ({ atLevel (lowpassed (noise (40.0, 7), 300.0), -18.0f), later });
            expectLessThan (TonalProfile::distance (profileOf (both, 30.0), profileOf (later)), 0.5f);
            expectGreaterThan (TonalProfile::distance (profileOf (both), profileOf (later)), 2.0f);
        }

        beginTest ("A mix ranks the reference that sounds like it first");
        {
            const std::vector<TonalProfile> references { profileOf (lowpassed (noise (20.0, 8), 800.0)),
                                                         profileOf (drums (20.0, 9)),
                                                         profileOf (highpassed (noise (20.0, 10), 2000.0)) };
            const auto mix = profileOf (scaled (drums (15.0, 11), -9.0f));
            std::vector<float> distances;
            for (const auto& reference : references)
                distances.push_back (TonalProfile::distance (mix, reference));
            expectEquals (static_cast<int> (std::min_element (distances.begin(), distances.end()) - distances.begin()), 1);
        }

        beginTest ("Block size doesn't matter");
        {
            const auto music = drums (10.0, 12);
            TonalAnalyser analyser (rate);
            for (size_t start = 0; start < music.size(); start += 517)
                analyser.process (music.data() + start, static_cast<int> (std::min<size_t> (517, music.size() - start)));
            expectLessThan (TonalProfile::distance (*analyser.getProfile(), profileOf (music)), 1.0e-4f);
        }
    }
};

static TonalProfileTests tonalProfileTests;

} // namespace lucky
