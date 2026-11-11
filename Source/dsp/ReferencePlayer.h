#pragma once

#include "Handoff.h"
#include "ref/ReferenceTrack.h"
#include <atomic>
#include <limits>

namespace lucky
{

/** Plays one reference's loop on the audio thread, crossfading at the loop point so the wrap is seamless. */
class ReferencePlayer
{
public:
    static constexpr double loopFadeSeconds = 0.01;

    /** What the player is playing: a track, its loop as samples and the loop's loudness. */
    struct Source
    {
        ReferenceTrackPtr track;
        juce::Range<juce::int64> loop;
        float loudness = -std::numeric_limits<float>::infinity();
    };

    /** Message thread: sets the track and loop to play; an empty track silences the player. */
    void setSource (ReferenceTrackPtr track, const LoopRegion& loop);

    /** Message thread: moves the playhead to the sample at the given time in the track at the next block. */
    void requestSeek (double seconds) noexcept { seekRequest.store (seconds); }

    /** Message thread: frees sources the audio thread no longer uses. */
    void collectGarbage() { sources.collectGarbage(); }

    /** Audio thread: picks up a new source or seek; call once at the start of each block. */
    const Source* beginBlock() noexcept;

    /** Audio thread: moves the playhead to the loop start. */
    void restart() noexcept;

    /** Audio thread: writes n samples of the loop into left and right and advances. */
    void render (float* left, float* right, int n) noexcept;

    /** Audio thread: advances n samples without producing audio. */
    void skip (int n) noexcept;

    /** Playhead in samples of the current track, for the UI. */
    juce::int64 getPosition() const noexcept { return position.load (std::memory_order_relaxed); }

private:
    juce::int64 fadeLength() const noexcept;
    juce::int64 step (juce::int64 from, int n) const noexcept;

    Handoff<Source> sources;
    const Source* source = nullptr;
    juce::int64 playhead = 0;
    std::atomic<juce::int64> position { 0 };
    std::atomic<double> seekRequest { -1.0 };
};

} // namespace lucky
