#include "ReferencePlayer.h"

namespace lucky
{

void ReferencePlayer::setSource (ReferenceTrackPtr track, const LoopRegion& loop)
{
    auto next = std::make_unique<Source>();
    if (track != nullptr)
    {
        next->loop = loop.resolve (track->getLength(), track->getSampleRate());
        next->loudness = track->getLoudness (loop);
    }
    next->track = std::move (track);
    sources.publish (std::move (next));
}

const ReferencePlayer::Source* ReferencePlayer::beginBlock() noexcept
{
    const auto* previous = source;
    source = sources.acquire();

    if (source != previous && source != nullptr)
    {
        const auto sameTrack = previous != nullptr && previous->track == source->track;
        if (! sameTrack || ! source->loop.contains (playhead))
            playhead = source->loop.getStart();
    }

    if (const auto seconds = seekRequest.exchange (-1.0); seconds >= 0.0 && source != nullptr && source->track != nullptr)
        playhead = juce::jlimit (source->loop.getStart(), source->loop.getEnd() - 1,
                                 static_cast<juce::int64> (seconds * source->track->getSampleRate()));

    position.store (playhead, std::memory_order_relaxed);
    return source;
}

void ReferencePlayer::restart() noexcept
{
    if (source != nullptr)
        playhead = source->loop.getStart();
    position.store (playhead, std::memory_order_relaxed);
}

juce::int64 ReferencePlayer::fadeLength() const noexcept
{
    return std::min<juce::int64> (static_cast<juce::int64> (loopFadeSeconds * source->track->getSampleRate()), source->loop.getLength() / 4);
}

juce::int64 ReferencePlayer::step (juce::int64 from, int n) const noexcept
{
    const auto next = from + n;
    const auto end = source->loop.getEnd();
    if (next < end)
        return next;

    const auto fade = fadeLength();
    return source->loop.getStart() + fade + (next - end) % std::max<juce::int64> (1, source->loop.getLength() - fade);
}

void ReferencePlayer::skip (int n) noexcept
{
    if (source != nullptr && source->track != nullptr && ! source->loop.isEmpty())
        playhead = step (playhead, n);
    position.store (playhead, std::memory_order_relaxed);
}

void ReferencePlayer::render (float* left, float* right, int n) noexcept
{
    if (source == nullptr || source->track == nullptr || source->loop.isEmpty())
    {
        juce::FloatVectorOperations::clear (left, n);
        juce::FloatVectorOperations::clear (right, n);
        return;
    }

    const auto& audio = source->track->getAudio();
    const auto* inL = audio.getReadPointer (0);
    const auto* inR = audio.getReadPointer (1);
    const auto start = source->loop.getStart();
    const auto fade = fadeLength();
    const auto fadeStart = source->loop.getEnd() - fade;

    for (int i = 0; i < n; ++i)
    {
        if (playhead >= fadeStart)
        {
            const auto t = static_cast<float> (playhead - fadeStart) / static_cast<float> (std::max<juce::int64> (1, fade));
            const auto out = std::cos (t * juce::MathConstants<float>::halfPi);
            const auto in = std::sin (t * juce::MathConstants<float>::halfPi);
            const auto head = start + (playhead - fadeStart);
            left[i] = inL[playhead] * out + inL[head] * in;
            right[i] = inR[playhead] * out + inR[head] * in;
        }
        else
        {
            left[i] = inL[playhead];
            right[i] = inR[playhead];
        }

        playhead = step (playhead, 1);
    }

    position.store (playhead, std::memory_order_relaxed);
}

} // namespace lucky
