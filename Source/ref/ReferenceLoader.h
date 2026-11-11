#pragma once

#include "ReferenceTrack.h"
#include <juce_events/juce_events.h>
#include <array>
#include <optional>

namespace lucky
{

/** Decodes reference files on a background thread, one pending request per slot; a newer request replaces an older one.
    Sends a change message when a result is ready to collect with takeResult(). */
class ReferenceLoader final : public juce::ChangeBroadcaster,
                              private juce::Thread
{
public:
    static constexpr int numSlots = 3;

    struct Result
    {
        juce::File file;
        ReferenceTrackPtr track;
        juce::String error;
    };

    ReferenceLoader();
    ~ReferenceLoader() override;

    /** Asks for the file to be loaded into the slot at sampleRate; an empty File clears the slot. */
    void request (int slot, const juce::File& file, double sampleRate);

    /** The finished load for the slot, if one is waiting, matching the most recent request. */
    std::optional<Result> takeResult (int slot);

    /** True while the slot's most recent request is still being decoded. */
    bool isLoading (int slot) const;

private:
    struct Request
    {
        juce::File file;
        double sampleRate = 0.0;
        juce::uint64 generation = 0;
    };

    struct SlotState
    {
        std::optional<Request> pending;
        juce::uint64 latestGeneration = 0;
        juce::uint64 completedGeneration = 0;
        std::optional<Result> result;
    };

    void run() override;

    juce::AudioFormatManager formats;
    mutable juce::CriticalSection lock;
    std::array<SlotState, numSlots> slots;
};

} // namespace lucky
