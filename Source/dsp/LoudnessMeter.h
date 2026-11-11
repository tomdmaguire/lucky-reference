#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace lucky
{

/** ITU-R BS.1770-4 gated integrated loudness of a mono or stereo signal.
    Streams audio through the K-weighting filter in 400 ms blocks every 100 ms and keeps the block energies in a fine
    histogram, so the integrated value needs no allocation after prepare() however long the measurement runs. */
class LoudnessMeter
{
public:
    /** Allocates the histogram and derives the K-weighting coefficients for the sample rate; resets the measurement. */
    void prepare (double sampleRate);

    /** Clears the measurement and the filter history. */
    void reset() noexcept;

    /** Measures numSamples samples of one or two channels; real-time safe. */
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** Gated integrated loudness in LUFS of everything measured since the last reset, or -infinity when no block has passed the absolute gate. */
    float getIntegrated() const noexcept;

    /** Seconds of audio, counted in 100 ms block steps, whose blocks passed the absolute gate. */
    double getGatedSeconds() const noexcept { return 0.1 * static_cast<double> (gatedBlocks); }

    /** Gated integrated loudness in LUFS of a section of a buffer at the given sample rate. */
    static float measure (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double sampleRate);

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
    int binFor (double blockEnergy) const noexcept;

    static constexpr float histogramTopLufs = 10.0f;
    static constexpr float binWidthLu = 0.05f;

    std::array<std::array<Biquad, 2>, 2> filters {};
    std::array<Biquad, 2> shelfTemplate {}, highPassTemplate {};
    int subBlockLength = 4800;
    int subBlockFill = 0;
    double subBlockSum = 0.0;
    std::array<double, 4> recentSubBlocks {};
    int subBlocksSeen = 0;
    juce::int64 gatedBlocks = 0;
    std::vector<double> binEnergy;
    std::vector<juce::int64> binCount;
};

} // namespace lucky
