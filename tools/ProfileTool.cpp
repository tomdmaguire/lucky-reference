#include "library/ReferenceLibrary.h"
#include <iostream>
#include <set>

/** Analyses audio files as the library does, printing how long each took, its punch and tilt, and how closely every
    pair matches, for checking the closest-match ranking against real music.

    With --stability, treats stretches of each file as a mix and reports how far a short stretch's profile is from the
    whole file's, and whether it ranks the other files the same way, to choose how much of the mix must be heard.

    usage: LuckyReferenceProfile [--stability] <file> [<file> ...] */
namespace
{
std::optional<lucky::TonalProfile> analyseStretch (const juce::File& file, juce::AudioFormatManager& formats, double startFraction, double seconds)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
        return std::nullopt;
    const auto length = static_cast<juce::int64> (seconds * reader->sampleRate);
    const auto start = static_cast<juce::int64> (startFraction * static_cast<double> (reader->lengthInSamples - length));
    if (start < 0)
        return std::nullopt;
    juce::AudioBuffer<float> buffer (2, static_cast<int> (length));
    reader->read (&buffer, 0, static_cast<int> (length), start, true, true);
    std::vector<float> mono (static_cast<size_t> (length));
    for (int i = 0; i < static_cast<int> (length); ++i)
        mono[static_cast<size_t> (i)] = 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i));
    lucky::TonalAnalyser analyser (reader->sampleRate);
    analyser.process (mono.data(), static_cast<int> (length));
    return analyser.getProfile();
}

std::vector<size_t> rankOthers (const lucky::TonalProfile& mix, const std::vector<lucky::TonalProfile>& library, size_t self)
{
    std::vector<size_t> order;
    for (size_t i = 0; i < library.size(); ++i)
        if (i != self)
            order.push_back (i);
    std::sort (order.begin(), order.end(), [&] (size_t a, size_t b) { return lucky::TonalProfile::distance (library[a], mix) < lucky::TonalProfile::distance (library[b], mix); });
    return order;
}

int stability (const juce::StringArray& paths, juce::AudioFormatManager& formats)
{
    std::vector<juce::File> files;
    std::vector<lucky::TonalProfile> full;
    for (const auto& path : paths)
        if (const auto profile = lucky::ReferenceLibrary::analyseFile (juce::File (path), formats))
        {
            files.emplace_back (path);
            full.push_back (*profile);
        }

    std::cout << "seconds  mean dB from whole song  worst  closest same  top-2 same\n";
    for (const auto seconds : { 5.0, 10.0, 15.0, 20.0, 30.0, 60.0 })
    {
        auto total = 0.0f, worst = 0.0f;
        auto count = 0, firstSame = 0, topTwoSame = 0;
        for (size_t i = 0; i < files.size(); ++i)
            for (const auto startFraction : { 0.2, 0.35, 0.5, 0.65, 0.8 })
                if (const auto stretch = analyseStretch (files[i], formats, startFraction, seconds))
                {
                    const auto distance = lucky::TonalProfile::distance (*stretch, full[i]);
                    total += distance;
                    worst = std::max (worst, distance);
                    const auto byStretch = rankOthers (*stretch, full, i), byWhole = rankOthers (full[i], full, i);
                    firstSame += byStretch[0] == byWhole[0] ? 1 : 0;
                    topTwoSame += (std::set<size_t> { byStretch[0], byStretch[1] } == std::set<size_t> { byWhole[0], byWhole[1] }) ? 1 : 0;
                    ++count;
                }
        std::cout << juce::String (seconds, 0).paddedLeft (' ', 7) << "  " << juce::String (total / static_cast<float> (count), 2).paddedLeft (' ', 22)
                  << "  " << juce::String (worst, 2).paddedLeft (' ', 5) << "  " << juce::String (100 * firstSame / count).paddedLeft (' ', 11) << "%"
                  << "  " << juce::String (100 * topTwoSame / count).paddedLeft (' ', 9) << "%\n";
    }
    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: LuckyReferenceProfile [--stability] <file> [<file> ...]\n";
        return 1;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    if (juce::String (argv[1]) == "--stability")
    {
        juce::StringArray paths;
        for (int i = 2; i < argc; ++i)
            paths.add (juce::String::fromUTF8 (argv[i]));
        return stability (paths, formats);
    }

    std::vector<std::pair<juce::String, lucky::TonalProfile>> profiles;
    for (int i = 1; i < argc; ++i)
    {
        const juce::File file (juce::String::fromUTF8 (argv[i]));
        const auto start = juce::Time::getMillisecondCounterHiRes();
        const auto profile = lucky::ReferenceLibrary::analyseFile (file, formats);
        const auto milliseconds = juce::Time::getMillisecondCounterHiRes() - start;
        if (! profile)
        {
            std::cout << file.getFileName() << ": no profile (unreadable or silent)\n";
            continue;
        }

        const auto low = (profile->bandsDb[2] + profile->bandsDb[3] + profile->bandsDb[4]) / 3.0f;
        const auto high = (profile->bandsDb[22] + profile->bandsDb[23] + profile->bandsDb[24]) / 3.0f;
        std::cout << juce::String (milliseconds, 0) << " ms  punch " << juce::String (profile->punchDb, 1) << " dB  lows-highs "
                  << juce::String (low - high, 1) << " dB  " << file.getFileName() << "\n";
        profiles.emplace_back (file.getFileNameWithoutExtension(), *profile);
    }

    std::cout << "\nmatch (distance in dB):\n";
    for (size_t a = 0; a < profiles.size(); ++a)
        for (size_t b = a + 1; b < profiles.size(); ++b)
            std::cout << "  " << juce::String (juce::roundToInt (100.0f * lucky::TonalProfile::match (profiles[a].second, profiles[b].second)))
                      << "% (" << juce::String (lucky::TonalProfile::distance (profiles[a].second, profiles[b].second), 1) << ")  "
                      << profiles[a].first << "  <->  " << profiles[b].first << "\n";
    return 0;
}
