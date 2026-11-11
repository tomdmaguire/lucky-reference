#include "ReferenceEngine.h"

namespace lucky
{

namespace
{
constexpr float silentLufs = -std::numeric_limits<float>::infinity();
}

void ReferenceEngine::prepare (double rate, int maximumBlockSize)
{
    sampleRate = rate;
    mixMeter.prepare (rate);
    lastMeasuredSeconds = 0.0;
    mixLoudness.store (silentLufs);
    scratch.setSize (2, maximumBlockSize, false, false, true);

    mixFader.level.reset (rate, sourceFadeSeconds);
    mixFader.level.setCurrentAndTargetValue (1.0f);
    for (auto& fader : referenceFaders)
    {
        fader.level.reset (rate, sourceFadeSeconds);
        fader.level.setCurrentAndTargetValue (0.0f);
    }
    for (auto& gain : gainsDb)
    {
        gain.reset (rate, gainRampSeconds);
        gain.setCurrentAndTargetValue (0.0f);
    }
    for (auto& loudness : referenceLoudness)
        loudness.store (silentLufs);

    wasPlaying = false;
    expectedTime.reset();
}

void ReferenceEngine::setSlot (int slot, ReferenceTrackPtr track, const LoopRegion& loop)
{
    players[static_cast<size_t> (slot)].setSource (std::move (track), loop);
}

void ReferenceEngine::collectGarbage()
{
    for (auto& player : players)
        player.collectGarbage();
}

void ReferenceEngine::followTransport (const Transport& transport, int numSamples) noexcept
{
    const auto started = transport.isPlaying && ! wasPlaying;
    wasPlaying = transport.isPlaying;

    if (started)
    {
        for (auto& player : players)
            player.restart();
        mixMeter.reset();
        lastMeasuredSeconds = 0.0;
    }
    else if (transport.isPlaying && transport.timeInSamples && expectedTime
             && std::abs (*transport.timeInSamples - *expectedTime) > numSamples)
    {
        auto wrapped = false;
        if (transport.loopPpq && transport.ppqPosition && transport.bpm && *transport.bpm > 0.0)
        {
            const auto blockPpq = numSamples / sampleRate * *transport.bpm / 60.0;
            const auto sinceLoopStart = *transport.ppqPosition - transport.loopPpq->first;
            wrapped = sinceLoopStart >= -1.0e-6 && sinceLoopStart <= blockPpq + 1.0e-6;
        }

        if (! wrapped)
        {
            mixMeter.reset();
            lastMeasuredSeconds = 0.0;
        }
    }

    expectedTime = transport.timeInSamples ? std::optional (*transport.timeInSamples + numSamples) : std::nullopt;
}

void ReferenceEngine::updateMatching (const Controls& controls, const std::array<const ReferencePlayer::Source*, numSlots>& sources) noexcept
{
    const auto measured = mixMeter.getGatedSeconds();
    if (measured >= minimumMixSeconds && measured > lastMeasuredSeconds)
    {
        lastMeasuredSeconds = measured;
        mixLoudness.store (mixMeter.getIntegrated());
    }

    const auto matchChanged = controls.match != wasMatching;
    wasMatching = controls.match;

    for (size_t slot = 0; slot < numSlots; ++slot)
    {
        const auto loudness = sources[slot] != nullptr ? sources[slot]->loudness : silentLufs;
        referenceLoudness[slot].store (loudness);

        const auto mix = mixLoudness.load();
        auto target = gainsDb[slot].getTargetValue();
        if (! controls.match)
            target = 0.0f;
        else if (std::isfinite (mix) && std::isfinite (loudness))
            target = juce::jlimit (-maximumGainDb, maximumGainDb, mix - loudness);

        if (matchChanged || referenceFaders[slot].isSilent())
            gainsDb[slot].setCurrentAndTargetValue (target);
        else
            gainsDb[slot].setTargetValue (target);
    }
}

void ReferenceEngine::process (juce::AudioBuffer<float>& buffer, const Controls& controls, const Transport& transport) noexcept
{
    const auto n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2)
        return;

    std::array<const ReferencePlayer::Source*, numSlots> sources {};
    for (size_t slot = 0; slot < numSlots; ++slot)
        sources[slot] = players[slot].beginBlock();

    followTransport (transport, n);
    if (transport.isPlaying)
        mixMeter.process (buffer.getArrayOfReadPointers(), 2, n);
    updateMatching (controls, sources);

    const auto selected = static_cast<size_t> (juce::jlimit (0, numSlots - 1, controls.slot));
    mixFader.level.setTargetValue (controls.reference ? 0.0f : 1.0f);
    for (size_t slot = 0; slot < numSlots; ++slot)
        referenceFaders[slot].level.setTargetValue (controls.reference && slot == selected ? 1.0f : 0.0f);

    auto referencesSilent = true;
    for (const auto& fader : referenceFaders)
        referencesSilent = referencesSilent && fader.isSilent();

    if (mixFader.isFull() && referencesSilent)
    {
        if (transport.isPlaying)
            for (auto& player : players)
                player.skip (n);
        return;
    }

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        const auto amplitude = mixFader.next();
        left[i] *= amplitude;
        right[i] *= amplitude;
    }

    for (size_t slot = 0; slot < numSlots; ++slot)
    {
        auto& fader = referenceFaders[slot];
        auto& gain = gainsDb[slot];
        if (fader.isSilent() || ! transport.isPlaying)
        {
            fader.level.skip (n);
            gain.skip (n);
            if (transport.isPlaying)
                players[slot].skip (n);
            continue;
        }

        auto* refLeft = scratch.getWritePointer (0);
        auto* refRight = scratch.getWritePointer (1);
        players[slot].render (refLeft, refRight, n);
        for (int i = 0; i < n; ++i)
        {
            const auto amplitude = fader.next() * juce::Decibels::decibelsToGain (gain.getNextValue());
            left[i] += refLeft[i] * amplitude;
            right[i] += refRight[i] * amplitude;
        }
    }
}

} // namespace lucky
