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
} // namespace

void LoudnessMeter::prepare (double sampleRate)
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
    const auto bins = static_cast<size_t> ((histogramTopLufs - absoluteGateLufs) / binWidthLu) + 1;
    binEnergy.assign (bins, 0.0);
    binCount.assign (bins, 0);
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
    std::fill (binEnergy.begin(), binEnergy.end(), 0.0);
    std::fill (binCount.begin(), binCount.end(), 0);
}

void LoudnessMeter::process (const float* const* channels, int numChannels, int numSamples) noexcept
{
    filterInto (channels, numChannels, numSamples, nullptr);
}

void LoudnessMeter::filterInto (const float* const* channels, int numChannels, int numSamples, std::vector<double>* slices)
{
    numChannels = std::min (numChannels, 2);

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& [shelf, highPass] = filters[static_cast<size_t> (ch)];
            const auto y = highPass.process (shelf.process (static_cast<double> (channels[ch][i])));
            subBlockSum += y * y;
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

    const auto blockEnergy = 0.25 * (recentSubBlocks[0] + recentSubBlocks[1] + recentSubBlocks[2] + recentSubBlocks[3]);
    if (blockEnergy <= 0.0 || energyToLufs (blockEnergy) <= absoluteGateLufs)
        return;

    const auto bin = static_cast<size_t> (binFor (blockEnergy));
    binEnergy[bin] += blockEnergy;
    ++binCount[bin];
    ++gatedBlocks;
}

int LoudnessMeter::binFor (double blockEnergy) const noexcept
{
    const auto position = (energyToLufs (blockEnergy) - absoluteGateLufs) / binWidthLu;
    return juce::jlimit (0, static_cast<int> (binCount.size()) - 1, static_cast<int> (position));
}

float LoudnessMeter::getIntegrated() const noexcept
{
    if (gatedBlocks == 0)
        return -std::numeric_limits<float>::infinity();

    double total = 0.0;
    for (const auto energy : binEnergy)
        total += energy;

    const auto threshold = lufsToEnergy (energyToLufs (total / static_cast<double> (gatedBlocks)) + relativeGateLu);
    const auto firstBin = static_cast<size_t> (binFor (threshold));

    double gatedTotal = 0.0;
    juce::int64 gatedCount = 0;
    for (auto bin = firstBin; bin < binCount.size(); ++bin)
    {
        gatedTotal += binEnergy[bin];
        gatedCount += binCount[bin];
    }

    return static_cast<float> (energyToLufs (gatedTotal / static_cast<double> (gatedCount)));
}

float LoudnessMeter::measure (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double sampleRate)
{
    LoudnessMeter meter;
    meter.prepare (sampleRate);

    const auto numChannels = std::min (buffer.getNumChannels(), 2);
    const float* channels[2] {};
    for (int ch = 0; ch < numChannels; ++ch)
        channels[ch] = buffer.getReadPointer (ch, startSample);

    meter.process (channels, numChannels, numSamples);
    return meter.getIntegrated();
}

std::vector<double> LoudnessMeter::sliceEnergies (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    LoudnessMeter meter;
    meter.prepare (sampleRate);

    std::vector<double> slices;
    slices.reserve (static_cast<size_t> (buffer.getNumSamples() / meter.subBlockLength + 1));
    meter.filterInto (buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples(), &slices);
    return slices;
}

float LoudnessMeter::integratedFromSlices (const double* energies, int count)
{
    std::vector<double> blocks;
    for (int i = 3; i < count; ++i)
    {
        const auto block = 0.25 * (energies[i - 3] + energies[i - 2] + energies[i - 1] + energies[i]);
        if (block > 0.0 && energyToLufs (block) > absoluteGateLufs)
            blocks.push_back (block);
    }

    if (blocks.empty())
        return -std::numeric_limits<float>::infinity();

    double total = 0.0;
    for (const auto block : blocks)
        total += block;

    const auto threshold = lufsToEnergy (energyToLufs (total / static_cast<double> (blocks.size())) + relativeGateLu);
    double gatedTotal = 0.0;
    int gatedCount = 0;
    for (const auto block : blocks)
        if (block > threshold)
        {
            gatedTotal += block;
            ++gatedCount;
        }

    return static_cast<float> (energyToLufs (gatedTotal / gatedCount));
}

} // namespace lucky
