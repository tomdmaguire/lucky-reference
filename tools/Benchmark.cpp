#include "dsp/ReferenceEngine.h"
#include <algorithm>
#include <iostream>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr double secondsToProcess = 20.0;

struct Result
{
    double averagePercent, p999Percent, worstPercent;
};

/** Four minutes of noisy "music" with a kick-like pulse, at the host rate. */
lucky::ReferenceTrackPtr makeTrack (int seed)
{
    juce::Random random (seed);
    juce::AudioBuffer<float> audio (2, static_cast<int> (sampleRate * 240.0));
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const auto pulse = static_cast<float> (std::exp (-std::fmod (i / sampleRate, 0.5) * 8.0));
        for (int ch = 0; ch < 2; ++ch)
            audio.setSample (ch, i, 0.2f * (random.nextFloat() * 2.0f - 1.0f) + 0.5f * pulse * static_cast<float> (std::sin (i * 0.006)));
    }
    return std::make_shared<const lucky::ReferenceTrack> (juce::File(), audio, sampleRate);
}

/** Runs secondsToProcess of noise through the engine and times every block against its real-time budget. */
Result measure (const std::array<lucky::ReferenceTrackPtr, 3>& tracks, int blockSize, bool reference, bool flipping)
{
    lucky::ReferenceEngine engine;
    engine.prepare (sampleRate, blockSize);
    for (int slot = 0; slot < 3; ++slot)
        engine.setSlot (slot, tracks[static_cast<size_t> (slot)], { 30.0 + slot * 20.0, 46.0 + slot * 20.0 });

    lucky::ReferenceEngine::Controls controls { reference, 1, true };
    lucky::ReferenceEngine::Transport transport;
    transport.isPlaying = true;
    transport.timeInSamples = 0;
    transport.bpm = 120.0;
    transport.ppqPosition = 0.0;

    juce::AudioBuffer<float> block (2, blockSize);
    juce::Random random (1);
    const auto blocks = static_cast<int> (secondsToProcess * sampleRate / blockSize);
    std::vector<double> percents;
    percents.reserve (static_cast<size_t> (blocks));
    const auto budget = blockSize / sampleRate;
    double total = 0.0;

    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                block.setSample (ch, i, random.nextFloat() * 0.2f - 0.1f);

        if (flipping && b % static_cast<int> (0.25 * sampleRate / blockSize) == 0)
        {
            controls.reference = ! controls.reference;
            controls.slot = (controls.slot + 1) % 3;
        }

        const auto start = juce::Time::getHighResolutionTicks();
        engine.process (block, controls, transport);
        const auto seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);

        total += seconds;
        percents.push_back (100.0 * seconds / budget);
        *transport.timeInSamples += blockSize;
        *transport.ppqPosition += blockSize / sampleRate * 2.0;

        if (b % 64 == 0)
            engine.collectGarbage();
    }

    std::sort (percents.begin(), percents.end());
    return { 100.0 * total / (blocks * budget), percents[static_cast<size_t> (percents.size() * 999 / 1000)], percents.back() };
}
} // namespace

/** CPU use of the reference engine at several buffer sizes, as a share of each block's real-time budget at 48 kHz. */
int main()
{
    const std::array tracks { makeTrack (1), makeTrack (2), makeTrack (3) };
    std::cout << "48 kHz, three 4-minute references, 20 s per run; % of each block's time budget\n";
    for (const auto blockSize : { 32, 64, 128, 512 })
        for (const auto& [name, reference, flipping] : { std::tuple { "mix", false, false }, std::tuple { "reference", true, false },
                                                         std::tuple { "flipping every 250 ms", true, true } })
        {
            const auto r = measure (tracks, blockSize, reference, flipping);
            std::cout << "block " << blockSize << ", " << name << ": average " << juce::String (r.averagePercent, 2) << "%, p99.9 "
                      << juce::String (r.p999Percent, 1) << "%, worst " << juce::String (r.worstPercent, 1) << "%\n";
        }
    return 0;
}
