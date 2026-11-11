#pragma once

#include "LoudnessMeter.h"
#include "ReferencePlayer.h"
#include <array>
#include <optional>

namespace lucky
{

/** The audio path: measures the mix, plays the references with the host transport, matches their loudness to the mix
    and crossfades between the mix and the selected reference. */
class ReferenceEngine
{
public:
    static constexpr int numSlots = 3;
    static constexpr double sourceFadeSeconds = 0.03;
    static constexpr double gainRampSeconds = 1.0;
    static constexpr double minimumMixSeconds = 1.5;
    static constexpr double readyFadeSeconds = 0.15;
    static constexpr float maximumGainDb = 24.0f;

    /** Per-block settings from the plugin's parameters. */
    struct Controls
    {
        bool reference = false;
        int slot = 0;
        bool match = true;
    };

    /** Per-block host transport state. */
    struct Transport
    {
        bool isPlaying = false;
        std::optional<juce::int64> timeInSamples;
        std::optional<double> ppqPosition;
        std::optional<double> bpm;
        std::optional<std::pair<double, double>> loopPpq;
    };

    /** Allocates buffers for the sample rate and block size; resets everything. */
    void prepare (double sampleRate, int maximumBlockSize);

    /** Message thread: sets the track and loop of a slot. */
    void setSlot (int slot, ReferenceTrackPtr track, const LoopRegion& loop);

    /** Message thread: moves a slot's playhead to a time in its track. */
    void seek (int slot, double seconds) noexcept { players[static_cast<size_t> (slot)].requestSeek (seconds); }

    /** Message thread: frees sources the audio thread has finished with. */
    void collectGarbage();

    /** Audio thread: processes a stereo block in place. */
    void process (juce::AudioBuffer<float>& buffer, const Controls& controls, const Transport& transport) noexcept;

    /** Mix loudness in LUFS over the last few seconds played (held across stops and jumps), or -infinity before the first estimate. */
    float getMixLoudness() const noexcept { return mixLoudness.load(); }

    /** Loudness in LUFS of a slot's loop, or -infinity when the slot is empty or silent. */
    float getReferenceLoudness (int slot) const noexcept { return referenceLoudness[static_cast<size_t> (slot)].load(); }

    /** Momentary loudness in LUFS of the mix's latest 400 ms while the transport plays, or -infinity. */
    float getMixMomentary() const noexcept { return mixMomentary.load(); }

    /** Gain in dB being applied to a slot's reference, including the fade-in before the first estimate (-infinity while silent). */
    float getAppliedGainDb (int slot) const noexcept { return appliedGainsDb[static_cast<size_t> (slot)].load(); }

    /** True while the host transport is playing. */
    bool isPlaying() const noexcept { return playing.load(); }

    /** True until the mix has been measured long enough to match against. */
    bool isMeasuring() const noexcept { return ! std::isfinite (getMixLoudness()); }

    /** Playhead of a slot in samples of its track. */
    juce::int64 getPosition (int slot) const noexcept { return players[static_cast<size_t> (slot)].getPosition(); }

private:
    struct Fader
    {
        juce::SmoothedValue<float> level;

        float next() noexcept { return std::sin (level.getNextValue() * juce::MathConstants<float>::halfPi); }
        bool isSilent() const noexcept { return ! level.isSmoothing() && level.getTargetValue() <= 0.0f; }
        bool isFull() const noexcept { return ! level.isSmoothing() && level.getTargetValue() >= 1.0f; }
    };

    void followTransport (const Transport& transport, int numSamples) noexcept;
    void publishGains() noexcept;
    void updateMatching (const Controls& controls, const std::array<const ReferencePlayer::Source*, numSlots>& sources) noexcept;

    double sampleRate = 48000.0;
    std::array<ReferencePlayer, numSlots> players;
    LoudnessMeter mixMeter;
    double lastMeasuredSeconds = 0.0;
    std::atomic<float> mixLoudness { -std::numeric_limits<float>::infinity() };
    std::array<std::atomic<float>, numSlots> referenceLoudness;
    Fader mixFader;
    std::array<Fader, numSlots> referenceFaders;
    std::array<juce::SmoothedValue<float>, numSlots> gainsDb;
    std::array<juce::SmoothedValue<float>, numSlots> readiness;
    std::atomic<float> mixMomentary { -std::numeric_limits<float>::infinity() };
    std::array<std::atomic<float>, numSlots> appliedGainsDb;
    bool wasMatching = true;
    bool wasPlaying = false;
    std::atomic<bool> playing { false };
    std::optional<juce::int64> expectedTime;
    juce::AudioBuffer<float> scratch;
};

} // namespace lucky
