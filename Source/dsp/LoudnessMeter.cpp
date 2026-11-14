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
    for (auto& filter : filters)
        filter.prepare (sampleRate);

    subBlockLength = std::max (1, juce::roundToInt (sampleRate * 0.1));
    fastLength = std::max (1, juce::roundToInt (sampleRate * fastSliceSeconds));
    blocks.assign (static_cast<size_t> (std::max (1, juce::roundToInt (windowSeconds * 10.0))), 0.0);
    reset();
}

void LoudnessMeter::reset() noexcept
{
    for (auto& filter : filters)
        filter.reset();

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
            const auto y = filters[static_cast<size_t> (ch)].process (static_cast<double> (channels[ch][i]));
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
