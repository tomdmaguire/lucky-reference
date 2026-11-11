#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace lucky
{

/** ITU-R BS.1770-4 gated loudness of a mono or stereo signal.
    Streams audio through the K-weighting filter in 400 ms blocks every 100 ms and keeps the most recent window of block
    energies, so the integrated value follows the last few seconds and needs no allocation after prepare(). */
class LoudnessMeter
{
public:
    /** Derives the K-weighting coefficients for the sample rate and holds windowSeconds of blocks; resets the measurement. */
    void prepare (double sampleRate, double windowSeconds = 10.0);

    /** Clears the measurement and the filter history. */
    void reset() noexcept;

    /** Measures numSamples samples of one or two channels; real-time safe. */
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** Gated integrated loudness in LUFS over the window, or -infinity when no block in it has passed the absolute gate. */
    float getIntegrated() const noexcept;

    /** Momentary loudness in LUFS of the latest 400 ms block, or -infinity before one is complete or when silent. */
    float getMomentary() const noexcept;

    /** K-weighted level in LUFS of the latest fastSliceSeconds, for lively displays, or -infinity when silent. */
    float getFast() const noexcept;

    /** Seconds of audio since the last reset, counted in 100 ms block steps, whose blocks passed the absolute gate. */
    double getGatedSeconds() const noexcept { return 0.1 * static_cast<double> (gatedBlocks); }

    /** Gated integrated loudness in LUFS of a section of a buffer at the given sample rate. */
    static float measure (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double sampleRate);

    /** K-weighted mean-square energy of each consecutive 100 ms slice of a buffer (a partial last slice is dropped),
        optionally also of each fastSliceSeconds slice. */
    static std::vector<double> sliceEnergies (const juce::AudioBuffer<float>& buffer, double sampleRate,
                                              std::vector<double>* fastSlices = nullptr);

    /** Gated integrated loudness in LUFS of consecutive 100 ms slice energies, or -infinity when too short or silent. */
    static float integratedFromSlices (const double* energies, int count);

    static constexpr double fastSliceSeconds = 0.025;
    static constexpr float absoluteGateLufs = -70.0f;
    static constexpr float relativeGateLu = -10.0f;

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        double process (double x) noexcept
        {
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    void finishSubBlock() noexcept;
    void filterInto (const float* const* channels, int numChannels, int numSamples, std::vector<double>* slices,
                     std::vector<double>* fastSlices);

    std::array<std::array<Biquad, 2>, 2> filters {};
    Biquad shelfTemplate, highPassTemplate;
    int subBlockLength = 4800;
    int fastLength = 1200;
    int fastFill = 0;
    double fastSum = 0.0;
    double latestFast = 0.0;
    int subBlockFill = 0;
    double subBlockSum = 0.0;
    std::array<double, 4> recentSubBlocks {};
    int subBlocksSeen = 0;
    juce::int64 gatedBlocks = 0;
    std::vector<double> blocks;
    size_t blocksHeld = 0;
    size_t nextBlock = 0;
    double latestBlock = 0.0;
};

} // namespace lucky
