#include "LoudnessMeter.h"
#include <cmath>
#include <limits>

namespace lucky
{

namespace
{
double energyToLufs (double energy) noexcept
{
    return -0.691 + 10.0 * std::log10 (energy);
}

double lufsToEnergy (double lufs) noexcept
{
    return std::pow (10.0, (lufs + 0.691) / 10.0);
}

bool passesAbsoluteGate (double block) noexcept
{
    return block > 0.0 && energyToLufs (block) > LoudnessMeter::absoluteGateLufs;
}

/** BS.1770 two-stage gating over 400 ms block energies in any order. */
float gatedLoudness (const double* blocks, size_t count) noexcept
{
    double total = 0.0;
    size_t passing = 0;
    for (size_t i = 0; i < count; ++i)
        if (passesAbsoluteGate (blocks[i]))
        {
            total += blocks[i];
            ++passing;
        }

    if (passing == 0)
        return -std::numeric_limits<float>::infinity();

    const auto threshold = lufsToEnergy (energyToLufs (total / static_cast<double> (passing)) + LoudnessMeter::relativeGateLu);
    double gatedTotal = 0.0;
    size_t gatedCount = 0;
    for (size_t i = 0; i < count; ++i)
        if (passesAbsoluteGate (blocks[i]) && blocks[i] > threshold)
        {
            gatedTotal += blocks[i];
            ++gatedCount;
        }

    return static_cast<float> (energyToLufs (gatedTotal / static_cast<double> (gatedCount)));
}
} // namespace

void LoudnessMeter::prepare (double sampleRate, double windowSeconds)
{
    const auto pi = juce::MathConstants<double>::pi;

    {
        const auto f0 = 1681.974450955533, gainDb = 3.999843853973347, q = 0.7071752369554196;
        const auto k = std::tan (pi * f0 / sampleRate);
        const auto vh = std::pow (10.0, gainDb / 20.0);
        const auto vb = std::pow (vh, 0.4996667741545416);
        const auto a0 = 1.0 + k / q + k * k;
        auto& shelf = shelfTemplate;
        shelf.b0 = (vh + vb * k / q + k * k) / a0;
        shelf.b1 = 2.0 * (k * k - vh) / a0;
        shelf.b2 = (vh - vb * k / q + k * k) / a0;
        shelf.a1 = 2.0 * (k * k - 1.0) / a0;
        shelf.a2 = (1.0 - k / q + k * k) / a0;
    }
    {
        const auto f0 = 38.13547087602444, q = 0.5003270373238773;
        const auto k = std::tan (pi * f0 / sampleRate);
        const auto a0 = 1.0 + k / q + k * k;
        auto& highPass = highPassTemplate;
        highPass.b0 = 1.0;
        highPass.b1 = -2.0;
        highPass.b2 = 1.0;
        highPass.a1 = 2.0 * (k * k - 1.0) / a0;
        highPass.a2 = (1.0 - k / q + k * k) / a0;
    }

    subBlockLength = std::max (1, juce::roundToInt (sampleRate * 0.1));
    fastLength = std::max (1, juce::roundToInt (sampleRate * fastSliceSeconds));
    blocks.assign (static_cast<size_t> (std::max (1, juce::roundToInt (windowSeconds * 10.0))), 0.0);
    reset();
}

void LoudnessMeter::reset() noexcept
{
    for (auto& channel : filters)
    {
        channel[0] = shelfTemplate;
        channel[1] = highPassTemplate;
    }

    subBlockFill = 0;
    subBlockSum = 0.0;
    recentSubBlocks = {};
    subBlocksSeen = 0;
    gatedBlocks = 0;
    blocksHeld = 0;
    nextBlock = 0;
    latestBlock = 0.0;
    fastFill = 0;
    fastSum = 0.0;
    latestFast = 0.0;
    std::fill (blocks.begin(), blocks.end(), 0.0);
}

void LoudnessMeter::process (const float* const* channels, int numChannels, int numSamples) noexcept
{
    filterInto (channels, numChannels, numSamples, nullptr, nullptr);
}

void LoudnessMeter::filterInto (const float* const* channels, int numChannels, int numSamples, std::vector<double>* slices,
                                std::vector<double>* fastSlices)
{
    numChannels = std::min (numChannels, 2);

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& [shelf, highPass] = filters[static_cast<size_t> (ch)];
            const auto y = highPass.process (shelf.process (static_cast<double> (channels[ch][i])));
            subBlockSum += y * y;
            fastSum += y * y;
        }

        if (++fastFill == fastLength)
        {
            latestFast = fastSum / static_cast<double> (fastLength);
            if (fastSlices != nullptr)
                fastSlices->push_back (latestFast);
            fastFill = 0;
            fastSum = 0.0;
        }

        if (++subBlockFill == subBlockLength)
        {
            if (slices != nullptr)
                slices->push_back (subBlockSum / static_cast<double> (subBlockLength));
            finishSubBlock();
        }
    }
}

void LoudnessMeter::finishSubBlock() noexcept
{
    recentSubBlocks[static_cast<size_t> (subBlocksSeen % 4)] = subBlockSum / static_cast<double> (subBlockLength);
    ++subBlocksSeen;
    subBlockFill = 0;
    subBlockSum = 0.0;

    if (subBlocksSeen < 4)
        return;

    latestBlock = 0.25 * (recentSubBlocks[0] + recentSubBlocks[1] + recentSubBlocks[2] + recentSubBlocks[3]);
    blocks[nextBlock] = latestBlock;
    nextBlock = (nextBlock + 1) % blocks.size();
    blocksHeld = std::min (blocksHeld + 1, blocks.size());
    if (passesAbsoluteGate (latestBlock))
        ++gatedBlocks;
}

float LoudnessMeter::getIntegrated() const noexcept
{
    return gatedLoudness (blocks.data(), blocksHeld);
}

float LoudnessMeter::getFast() const noexcept
{
    return latestFast > 0.0 ? static_cast<float> (energyToLufs (latestFast)) : -std::numeric_limits<float>::infinity();
}

float LoudnessMeter::getMomentary() const noexcept
{
    return subBlocksSeen >= 4 && latestBlock > 0.0 ? static_cast<float> (energyToLufs (latestBlock)) : -std::numeric_limits<float>::infinity();
}

float LoudnessMeter::measure (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double sampleRate)
{
    const juce::AudioBuffer<float> section (const_cast<float* const*> (buffer.getArrayOfReadPointers()), std::min (buffer.getNumChannels(), 2),
                                            startSample, numSamples);
    const auto slices = sliceEnergies (section, sampleRate);
    return integratedFromSlices (slices.data(), static_cast<int> (slices.size()));
}

std::vector<double> LoudnessMeter::sliceEnergies (const juce::AudioBuffer<float>& buffer, double sampleRate, std::vector<double>* fastSlices)
{
    LoudnessMeter meter;
    meter.prepare (sampleRate);

    std::vector<double> slices;
    slices.reserve (static_cast<size_t> (buffer.getNumSamples() / meter.subBlockLength + 1));
    if (fastSlices != nullptr)
        fastSlices->reserve (static_cast<size_t> (buffer.getNumSamples() / meter.fastLength + 1));
    meter.filterInto (buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples(), &slices, fastSlices);
    return slices;
}

float LoudnessMeter::integratedFromSlices (const double* energies, int count)
{
    std::vector<double> blockEnergies;
    for (int i = 3; i < count; ++i)
        blockEnergies.push_back (0.25 * (energies[i - 3] + energies[i - 2] + energies[i - 1] + energies[i]));
    return gatedLoudness (blockEnergies.data(), blockEnergies.size());
}

} // namespace lucky
