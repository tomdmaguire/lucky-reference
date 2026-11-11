#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>

namespace lucky
{

/** Passes immutable objects from the message thread to the audio thread without locks or audio-thread frees.
    The audio thread adopts the newest published object; ones it lets go of are queued and deleted by collectGarbage(). */
template <typename T>
class Handoff
{
public:
    Handoff() = default;

    ~Handoff()
    {
        delete pending.exchange (nullptr);
        delete current;
        collectGarbage();
    }

    /** Message thread: makes the object the next one the audio thread will use. */
    void publish (std::unique_ptr<const T> object)
    {
        delete pending.exchange (object.release());
        collectGarbage();
    }

    /** Audio thread: the newest published object, or nullptr if none has been published. */
    const T* acquire() noexcept
    {
        if (auto* next = pending.exchange (nullptr))
        {
            if (current != nullptr)
                retire (current);
            current = next;
        }
        return current;
    }

    /** Message thread: deletes objects the audio thread has finished with. */
    void collectGarbage()
    {
        const auto scope = retired.read (retired.getNumReady());
        scope.forEach ([this] (int index) { delete std::exchange (retiredItems[static_cast<size_t> (index)], nullptr); });
    }

private:
    void retire (const T* object) noexcept
    {
        const auto scope = retired.write (1);
        if (scope.blockSize1 + scope.blockSize2 == 1)
            scope.forEach ([this, object] (int index) { retiredItems[static_cast<size_t> (index)] = object; });
        else
            jassertfalse; // Garbage isn't being collected; the object leaks rather than being freed on the audio thread.
    }

    static constexpr int capacity = 64;
    std::atomic<const T*> pending { nullptr };
    const T* current = nullptr;
    juce::AbstractFifo retired { capacity };
    std::array<const T*, capacity> retiredItems {};

    JUCE_DECLARE_NON_COPYABLE (Handoff)
};

} // namespace lucky
