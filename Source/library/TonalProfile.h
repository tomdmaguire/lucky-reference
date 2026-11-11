#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <deque>
#include <optional>

namespace lucky
{

/** How a piece of music sounds, independent of its loudness: the balance across third-octave bands and how punchy it is.
    Two profiles can be compared to rank references by how close they are to a mix. */
struct TonalProfile
{
    /** Third-octave bands from 31.5 Hz to 12.5 kHz. They stop below 16 kHz on purpose: lossy files cut the top end
        there, and a lossy reference shouldn't count as different just for that. */
    static constexpr int numBands = 27;

    /** Band level in dB relative to the average band, so overall loudness has no effect. */
    std::array<float, numBands> bandsDb {};

    /** Median peak-to-RMS ratio of one-second windows, in dB: lower means denser, more limited material. */
    float punchDb = 0.0f;

    /** Centre frequency of a band in Hz. */
    static double bandCentreHz (int band);

    /** How far apart two profiles sound, in dB: 0 for identical, a few dB for different productions. */
    static float distance (const TonalProfile& a, const TonalProfile& b);

    /** A 0-1 score for display, 1 for identical profiles, falling smoothly with distance. */
    static float match (const TonalProfile& a, const TonalProfile& b);
};

/** Builds a TonalProfile from mono audio fed in any block size. Audio below an absolute gate (-70 dBFS) or a relative
    gate (10 dB under the average), judged over a second at a time, is ignored, so silence and quiet intros don't skew
    the result while the gaps between drum hits still count. With a window set,
    only the most recent audio counts, so the profile follows a mix as it changes. */
class TonalAnalyser
{
public:
    /** @param windowSeconds how much recent audio to keep, or 0 to keep everything. */
    explicit TonalAnalyser (double sampleRate, double windowSeconds = 0.0);

    void process (const float* samples, int numSamples);
    void reset();

    double getSampleRate() const noexcept { return rate; }

    /** Seconds of audio that pass the gates, within the window. */
    double getGatedSeconds() const;

    /** The profile so far, or nothing until any audio passes the gates. */
    std::optional<TonalProfile> getProfile() const;

private:
    struct Frame
    {
        std::array<float, TonalProfile::numBands> bandEnergy {};
        float energy = 0.0f;
    };

    struct PunchBlock
    {
        float crestDb = 0.0f;
        float energy = 0.0f;
    };

    /** Frames or blocks either side of one that make up a second of context for the gate. */
    int contextRadius (int step) const;
    void analyseFrame();
    void finishPunchBlock();

    const double rate;
    const int fftOrder, fftSize, hopSize, punchBlockSize;
    const size_t maxFrames, maxPunchBlocks;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<std::pair<int, int>, TonalProfile::numBands> bandBins {};
    std::vector<float> input, scratch;
    int inputFill = 0;
    std::deque<Frame> frames;
    std::deque<PunchBlock> punchBlocks;
    float punchPeak = 0.0f, punchSum = 0.0f;
    int punchFill = 0;
};

} // namespace lucky
