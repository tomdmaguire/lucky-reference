#include "Resampler.h"
#include <AudioToolbox/AudioToolbox.h>

namespace lucky
{

namespace
{
AudioStreamBasicDescription stereoFloat (double rate)
{
    AudioStreamBasicDescription description {};
    description.mSampleRate = rate;
    description.mFormatID = kAudioFormatLinearPCM;
    description.mFormatFlags = static_cast<UInt32> (kAudioFormatFlagsNativeFloatPacked) | static_cast<UInt32> (kAudioFormatFlagIsNonInterleaved);
    description.mBytesPerPacket = sizeof (float);
    description.mFramesPerPacket = 1;
    description.mBytesPerFrame = sizeof (float);
    description.mChannelsPerFrame = 2;
    description.mBitsPerChannel = 32;
    return description;
}

/** Hands the converter the whole source once, then reports the end of the input. */
struct Feed
{
    const juce::AudioBuffer<float>& source;
    bool delivered = false;

    static OSStatus provide (AudioConverterRef, UInt32* numPackets, AudioBufferList* data, AudioStreamPacketDescription**, void* userData)
    {
        auto& feed = *static_cast<Feed*> (userData);
        if (feed.delivered)
        {
            *numPackets = 0;
            for (UInt32 i = 0; i < data->mNumberBuffers; ++i)
                data->mBuffers[i].mDataByteSize = 0;
            return noErr;
        }

        const auto frames = static_cast<UInt32> (feed.source.getNumSamples());
        for (UInt32 ch = 0; ch < data->mNumberBuffers; ++ch)
        {
            data->mBuffers[ch].mNumberChannels = 1;
            data->mBuffers[ch].mData = const_cast<float*> (feed.source.getReadPointer (static_cast<int> (ch)));
            data->mBuffers[ch].mDataByteSize = frames * sizeof (float);
        }
        *numPackets = frames;
        feed.delivered = true;
        return noErr;
    }
};
} // namespace

juce::AudioBuffer<float> resampleStereo (const juce::AudioBuffer<float>& source, double sourceRate, double targetRate)
{
    if (std::abs (sourceRate - targetRate) < 0.5 || source.getNumSamples() == 0)
        return source;

    const auto input = stereoFloat (sourceRate), output = stereoFloat (targetRate);
    AudioConverterRef converter = nullptr;
    if (AudioConverterNew (&input, &output, &converter) != noErr)
        return {};

    UInt32 complexity = kAudioConverterSampleRateConverterComplexity_Normal;
    AudioConverterSetProperty (converter, kAudioConverterSampleRateConverterComplexity, sizeof (complexity), &complexity);
    UInt32 quality = kAudioConverterQuality_Max;
    AudioConverterSetProperty (converter, kAudioConverterSampleRateConverterQuality, sizeof (quality), &quality);

    const auto expected = static_cast<int> (std::ceil (source.getNumSamples() * targetRate / sourceRate));
    juce::AudioBuffer<float> result (2, expected + 64);
    Feed feed { source };
    int written = 0;

    while (written < result.getNumSamples())
    {
        auto frames = static_cast<UInt32> (std::min (65536, result.getNumSamples() - written));
        alignas (AudioBufferList) std::byte storage[sizeof (AudioBufferList) + sizeof (::AudioBuffer)] {};
        auto* list = reinterpret_cast<AudioBufferList*> (storage);
        list->mNumberBuffers = 2;
        for (UInt32 ch = 0; ch < 2; ++ch)
        {
            list->mBuffers[ch].mNumberChannels = 1;
            list->mBuffers[ch].mData = result.getWritePointer (static_cast<int> (ch), written);
            list->mBuffers[ch].mDataByteSize = frames * sizeof (float);
        }

        const auto status = AudioConverterFillComplexBuffer (converter, Feed::provide, &feed, &frames, list, nullptr);
        written += static_cast<int> (frames);
        if (status != noErr || frames == 0)
            break;
    }

    AudioConverterDispose (converter);
    result.setSize (2, std::min (written, expected), true);
    return result;
}

} // namespace lucky
