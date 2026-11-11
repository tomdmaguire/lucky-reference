#include "TonalProfile.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace lucky
{

namespace
{
constexpr int firstBandOffset = -15;
constexpr float absoluteGateEnergy = 1.0e-7f;
constexpr float relativeGate = 0.1f;
constexpr double punchBlockSeconds = 1.0;
constexpr double gateContextSeconds = 1.0;
constexpr float matchScaleDb = 9.0f;

/** Which frames or blocks count. Each is judged by the average energy of the second of audio around it, so the gaps
    between drum hits count but quiet passages don't: it must be above -70 dBFS and within 10 dB of the average of
    those that are. */
template <typename Item>
std::vector<bool> gate (const std::deque<Item>& items, int radius)
{
    const auto count = items.size();
    std::vector<double> prefix (count + 1, 0.0);
    for (size_t i = 0; i < count; ++i)
        prefix[i + 1] = prefix[i] + items[i].energy;

    std::vector<float> context (count);
    for (size_t i = 0; i < count; ++i)
    {
        const auto first = i > static_cast<size_t> (radius) ? i - static_cast<size_t> (radius) : 0;
        const auto last = std::min (count, i + static_cast<size_t> (radius) + 1);
        context[i] = static_cast<float> ((prefix[last] - prefix[first]) / static_cast<double> (last - first));
    }

    double sum = 0.0;
    size_t loud = 0;
    for (const auto energy : context)
        if (energy > absoluteGateEnergy)
        {
            sum += energy;
            ++loud;
        }

    std::vector<bool> passes (count, false);
    if (loud == 0)
        return passes;
    const auto threshold = std::max (absoluteGateEnergy, static_cast<float> (sum / static_cast<double> (loud)) * relativeGate);
    for (size_t i = 0; i < count; ++i)
        passes[i] = context[i] >= threshold;
    return passes;
}

size_t capacityFor (double windowSeconds, double rate, int step)
{
    if (windowSeconds <= 0.0)
        return std::numeric_limits<size_t>::max();
    return static_cast<size_t> (std::ceil (windowSeconds * rate / step));
}
} // namespace

double TonalProfile::bandCentreHz (int band)
{
    return 1000.0 * std::pow (2.0, (band + firstBandOffset) / 3.0);
}

float TonalProfile::distance (const TonalProfile& a, const TonalProfile& b)
{
    auto squares = 0.0f;
    for (int i = 0; i < numBands; ++i)
    {
        const auto difference = a.bandsDb[static_cast<size_t> (i)] - b.bandsDb[static_cast<size_t> (i)];
        squares += difference * difference;
    }
    const auto punch = a.punchDb - b.punchDb;
    return std::sqrt (squares / static_cast<float> (numBands) + punch * punch);
}

float TonalProfile::match (const TonalProfile& a, const TonalProfile& b)
{
    return std::exp (-distance (a, b) / matchScaleDb);
}

TonalAnalyser::TonalAnalyser (double sampleRate, double windowSeconds)
    : rate (sampleRate),
      fftOrder (sampleRate > 64000.0 ? 14 : 13),
      fftSize (1 << fftOrder),
      hopSize (fftSize / 2),
      punchBlockSize (std::max (1, static_cast<int> (std::round (punchBlockSeconds * sampleRate)))),
      maxFrames (capacityFor (windowSeconds, sampleRate, hopSize)),
      maxPunchBlocks (capacityFor (windowSeconds, sampleRate, punchBlockSize)),
      fft (fftOrder),
      window (static_cast<size_t> (fftSize), juce::dsp::WindowingFunction<float>::hann, false),
      input (static_cast<size_t> (fftSize)),
      scratch (static_cast<size_t> (fftSize * 2))
{
    const auto binHz = rate / fftSize;
    const auto lastBin = fftSize / 2;
    for (int band = 0; band < TonalProfile::numBands; ++band)
    {
        const auto centre = TonalProfile::bandCentreHz (band);
        const auto low = std::min (lastBin, static_cast<int> (std::ceil (centre * std::pow (2.0, -1.0 / 6.0) / binHz)));
        const auto high = std::min (lastBin, static_cast<int> (std::floor (centre * std::pow (2.0, 1.0 / 6.0) / binHz)));
        bandBins[static_cast<size_t> (band)] = { low, std::max (low, high) };
    }
}

void TonalAnalyser::reset()
{
    inputFill = 0;
    frames.clear();
    punchBlocks.clear();
    punchPeak = punchSum = 0.0f;
    punchFill = 0;
}

void TonalAnalyser::process (const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const auto sample = samples[i];

        input[static_cast<size_t> (inputFill++)] = sample;
        if (inputFill == fftSize)
        {
            analyseFrame();
            std::copy (input.begin() + hopSize, input.end(), input.begin());
            inputFill = fftSize - hopSize;
        }

        punchPeak = std::max (punchPeak, std::abs (sample));
        punchSum += sample * sample;
        if (++punchFill == punchBlockSize)
            finishPunchBlock();
    }
}

void TonalAnalyser::analyseFrame()
{
    Frame frame;
    frame.energy = std::inner_product (input.begin(), input.end(), input.begin(), 0.0f) / static_cast<float> (fftSize);

    std::fill (scratch.begin(), scratch.end(), 0.0f);
    std::copy (input.begin(), input.end(), scratch.begin());
    window.multiplyWithWindowingTable (scratch.data(), static_cast<size_t> (fftSize));
    fft.performFrequencyOnlyForwardTransform (scratch.data(), true);

    for (size_t band = 0; band < bandBins.size(); ++band)
    {
        auto energy = 0.0f;
        for (int bin = bandBins[band].first; bin <= bandBins[band].second; ++bin)
            energy += scratch[static_cast<size_t> (bin)] * scratch[static_cast<size_t> (bin)];
        frame.bandEnergy[band] = energy;
    }

    frames.push_back (frame);
    if (frames.size() > maxFrames)
        frames.pop_front();
}

void TonalAnalyser::finishPunchBlock()
{
    const auto energy = punchSum / static_cast<float> (punchFill);
    const auto rms = std::sqrt (energy);
    punchBlocks.push_back ({ rms > 1.0e-6f ? 20.0f * std::log10 (punchPeak / rms) : 0.0f, energy });
    if (punchBlocks.size() > maxPunchBlocks)
        punchBlocks.pop_front();
    punchPeak = punchSum = 0.0f;
    punchFill = 0;
}

int TonalAnalyser::contextRadius (int step) const
{
    return std::max (1, static_cast<int> (std::round (gateContextSeconds * 0.5 * rate / step)));
}

double TonalAnalyser::getGatedSeconds() const
{
    const auto passes = gate (frames, contextRadius (hopSize));
    return static_cast<double> (std::count (passes.begin(), passes.end(), true)) * hopSize / rate;
}

std::optional<TonalProfile> TonalAnalyser::getProfile() const
{
    const auto passes = gate (frames, contextRadius (hopSize));
    std::array<double, TonalProfile::numBands> sums {};
    size_t count = 0;
    for (size_t i = 0; i < frames.size(); ++i)
        if (passes[i])
        {
            for (size_t band = 0; band < sums.size(); ++band)
                sums[band] += frames[i].bandEnergy[band];
            ++count;
        }
    if (count == 0)
        return std::nullopt;

    TonalProfile profile;
    auto mean = 0.0f;
    for (size_t band = 0; band < sums.size(); ++band)
    {
        profile.bandsDb[band] = 10.0f * std::log10 (static_cast<float> (sums[band] / static_cast<double> (count)) + 1.0e-20f);
        mean += profile.bandsDb[band];
    }
    mean /= static_cast<float> (TonalProfile::numBands);
    for (auto& level : profile.bandsDb)
        level -= mean;

    const auto punchPasses = gate (punchBlocks, contextRadius (punchBlockSize));
    std::vector<float> crests;
    for (size_t i = 0; i < punchBlocks.size(); ++i)
        if (punchPasses[i])
            crests.push_back (punchBlocks[i].crestDb);
    if (! crests.empty())
    {
        const auto middle = crests.begin() + static_cast<std::ptrdiff_t> (crests.size() / 2);
        std::nth_element (crests.begin(), middle, crests.end());
        profile.punchDb = *middle;
    }
    return profile;
}

} // namespace lucky
