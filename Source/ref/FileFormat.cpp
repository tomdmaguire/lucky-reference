#include "FileFormat.h"
#include <AudioToolbox/AudioToolbox.h>

namespace lucky
{

namespace
{
constexpr UInt32 losslessM4aMinimumBitRate = 500000;

UInt32 readBitRate (const juce::File& file)
{
    const auto path = file.getFullPathName().toRawUTF8();
    const auto url = CFURLCreateFromFileSystemRepresentation (nullptr, reinterpret_cast<const UInt8*> (path), static_cast<CFIndex> (std::strlen (path)), false);
    if (url == nullptr)
        return 0;

    AudioFileID audioFile = nullptr;
    UInt32 bitRate = 0;
    if (AudioFileOpenURL (url, kAudioFileReadPermission, 0, &audioFile) == noErr)
    {
        UInt32 size = sizeof (bitRate);
        if (AudioFileGetProperty (audioFile, kAudioFilePropertyBitRate, &size, &bitRate) != noErr)
            bitRate = 0;
        AudioFileClose (audioFile);
    }
    CFRelease (url);
    return bitRate;
}
} // namespace

juce::String describeFileFormat (const juce::File& file)
{
    const auto extension = file.getFileExtension().fromFirstOccurrenceOf (".", false, false).toUpperCase();
    if (extension.isEmpty())
        return {};

    const auto alwaysLossy = juce::StringArray { "MP3", "AAC", "OGG", "OPUS" }.contains (extension);
    if (! alwaysLossy && extension != "M4A" && extension != "MP4")
        return extension;

    const auto bitRate = readBitRate (file);
    if (! alwaysLossy && (bitRate == 0 || bitRate >= losslessM4aMinimumBitRate))
        return extension;
    if (bitRate == 0)
        return extension;
    return extension + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + juce::String (juce::roundToInt (bitRate / 1000.0)) + " kbps";
}

} // namespace lucky
