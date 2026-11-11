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
    tapRate.store (rate);
    mixMeter.prepare (rate);
    lastMeasuredSeconds = 0.0;
    mixLoudness.store (silentLufs);
    scratch.setSize (2, std::max (1, maximumBlockSize), false, false, true);

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
    for (auto& ready : readiness)
    {
        ready.reset (rate, readyFadeSeconds);
        ready.setCurrentAndTargetValue (0.0f);
    }
    for (auto& applied : appliedGainsDb)
        applied.store (silentLufs);
    mixFast.store (silentLufs);
    for (auto& loudness : referenceLoudness)
        loudness.store (silentLufs);

    wasPlaying = false;
    expectedTime.reset();
}

int ReferenceEngine::readMixTap (float* dest, int maxSamples) noexcept
{
    const auto read = mixTapFifo.read (std::min (maxSamples, mixTapFifo.getNumReady()));
    std::copy_n (mixTap.begin() + read.startIndex1, read.blockSize1, dest);
    std::copy_n (mixTap.begin() + read.startIndex2, read.blockSize2, dest + read.blockSize1);
    return read.blockSize1 + read.blockSize2;
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
    const auto hadEstimate = std::isfinite (mixLoudness.load());
    const auto measured = mixMeter.getGatedSeconds();
    if (measured >= minimumMixSeconds && measured > lastMeasuredSeconds)
    {
        lastMeasuredSeconds = measured;
        mixLoudness.store (mixMeter.getIntegrated());
    }
    const auto firstEstimate = ! hadEstimate && std::isfinite (mixLoudness.load());

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

        if (matchChanged || firstEstimate || referenceFaders[slot].isSilent())
            gainsDb[slot].setCurrentAndTargetValue (target);
        else
            gainsDb[slot].setTargetValue (target);

        readiness[slot].setTargetValue (! controls.match || std::isfinite (mix) ? 1.0f : 0.0f);
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
    {
        mixMeter.process (buffer.getArrayOfReadPointers(), 2, n);
        const auto write = mixTapFifo.write (std::min (n, mixTapFifo.getFreeSpace()));
        const auto* left = buffer.getReadPointer (0);
        const auto* right = buffer.getReadPointer (1);
        for (int i = 0; i < write.blockSize1; ++i)
            mixTap[static_cast<size_t> (write.startIndex1 + i)] = 0.5f * (left[i] + right[i]);
        for (int i = 0; i < write.blockSize2; ++i)
            mixTap[static_cast<size_t> (write.startIndex2 + i)] = 0.5f * (left[write.blockSize1 + i] + right[write.blockSize1 + i]);
    }
    mixFast.store (transport.isPlaying ? mixMeter.getFast() : silentLufs);
    playing.store (transport.isPlaying);
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
        for (size_t slot = 0; slot < numSlots; ++slot)
        {
            gainsDb[slot].skip (n);
            readiness[slot].skip (n);
            if (transport.isPlaying)
                players[slot].skip (n);
        }
        publishGains();
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
        auto& ready = readiness[slot];
        if (fader.isSilent() || ! transport.isPlaying)
        {
            fader.level.skip (n);
            gain.skip (n);
            ready.skip (n);
            if (transport.isPlaying)
                players[slot].skip (n);
            continue;
        }

        auto* refLeft = scratch.getWritePointer (0);
        auto* refRight = scratch.getWritePointer (1);
        for (int offset = 0; offset < n; offset += scratch.getNumSamples())
        {
            const auto chunk = std::min (scratch.getNumSamples(), n - offset);
            players[slot].render (refLeft, refRight, chunk);
            for (int i = 0; i < chunk; ++i)
            {
                const auto amplitude = fader.next() * ready.getNextValue() * juce::Decibels::decibelsToGain (gain.getNextValue());
                left[offset + i] += refLeft[i] * amplitude;
                right[offset + i] += refRight[i] * amplitude;
            }
        }
    }

    publishGains();
}

void ReferenceEngine::publishGains() noexcept
{
    for (size_t slot = 0; slot < numSlots; ++slot)
    {
        const auto ready = readiness[slot].getCurrentValue();
        appliedGainsDb[slot].store (ready > 0.0f ? gainsDb[slot].getCurrentValue() + juce::Decibels::gainToDecibels (ready, -100.0f) : silentLufs);
    }
}

} // namespace lucky