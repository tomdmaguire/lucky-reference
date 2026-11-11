#include "ReferenceLoader.h"

namespace lucky
{

ReferenceLoader::ReferenceLoader() : juce::Thread ("Lucky Reference loader")
{
    formats.registerBasicFormats();
    startThread (juce::Thread::Priority::background);
}

ReferenceLoader::~ReferenceLoader()
{
    stopThread (10000);
}

void ReferenceLoader::request (int slot, const juce::File& file, double sampleRate)
{
    {
        const juce::ScopedLock scope (lock);
        auto& state = slots[static_cast<size_t> (slot)];
        state.pending = Request { file, sampleRate, ++state.latestGeneration };
        state.result.reset();
    }
    notify();
}

std::optional<ReferenceLoader::Result> ReferenceLoader::takeResult (int slot)
{
    const juce::ScopedLock scope (lock);
    return std::exchange (slots[static_cast<size_t> (slot)].result, std::nullopt);
}

bool ReferenceLoader::isLoading (int slot) const
{
    const juce::ScopedLock scope (lock);
    const auto& state = slots[static_cast<size_t> (slot)];
    return state.completedGeneration != state.latestGeneration;
}

void ReferenceLoader::run()
{
    while (! threadShouldExit())
    {
        std::optional<std::pair<int, Request>> job;
        {
            const juce::ScopedLock scope (lock);
            for (int slot = 0; slot < numSlots && ! job; ++slot)
                if (auto& pending = slots[static_cast<size_t> (slot)].pending)
                    job = { slot, *std::exchange (pending, std::nullopt) };
        }

        if (! job)
        {
            wait (-1);
            continue;
        }

        const auto& [slot, request] = *job;
        Result result { request.file, nullptr, {} };
        if (request.file != juce::File())
            result.track = ReferenceTrack::load (request.file, request.sampleRate, formats, result.error);

        {
            const juce::ScopedLock scope (lock);
            auto& state = slots[static_cast<size_t> (slot)];
            if (state.latestGeneration != request.generation)
                continue;

            state.completedGeneration = request.generation;
            state.result = std::move (result);
        }
        sendChangeMessage();
    }
}

} // namespace lucky
