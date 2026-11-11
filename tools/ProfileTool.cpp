#include "library/ReferenceLibrary.h"
#include <iostream>

/** Analyses audio files as the library does, printing how long each took, its punch and tilt, and how closely every
    pair matches, for checking the closest-match ranking against real music.

    usage: LuckyReferenceProfile <file> [<file> ...] */
int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: LuckyReferenceProfile <file> [<file> ...]\n";
        return 1;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

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
