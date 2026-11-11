#pragma once

#include "TonalProfile.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <map>
#include <set>

namespace lucky
{

/** A reference track in the library folder. */
struct LibraryEntry
{
    juce::File file;
    /** Path inside the library with forward slashes, e.g. "Club/Track.wav"; identifies the entry. */
    juce::String key;
    /** File name without its extension. */
    juce::String name;
    /** Subfolder inside the library, e.g. "Club", or empty at the top level. */
    juce::String folder;
    /** Badge text from describeFileFormat, empty until the details are read. */
    juce::String format;
    /** Length in seconds, 0 until the details are read. */
    double seconds = 0.0;
    /** How the track sounds, once analysed; stays empty for silent or unreadable files. */
    std::optional<TonalProfile> profile;
    /** True once the file's details and profile have been read. */
    bool analysed = false;
    bool favourite = false;
};

/** The reference library: a folder of reference tracks shared by every plugin instance. A rescan lists the folder
    (including subfolders) on the calling thread, then a low-priority thread reads each new or changed file's details,
    which are cached so they are only read once. Favourites are kept separately from the cache, so clearing the cache
    never loses them. */
class ReferenceLibrary final : public juce::ChangeBroadcaster,
                               private juce::Thread
{
public:
    /** The library in ~/Music/Lucky Reference/References (or $LUCKY_REFERENCE_LIBRARY), with its cache and favourites
        in ~/Library/Application Support/Lucky Reference (or $LUCKY_REFERENCE_STATE). */
    ReferenceLibrary();

    /** A library in any folder, keeping its cache and favourites in stateFolder. */
    ReferenceLibrary (const juce::File& root, const juce::File& stateFolder);

    ~ReferenceLibrary() override;

    static inline const juce::String audioExtensions { "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.aac;*.caf" };

    /** The library folder, created if it doesn't exist. */
    juce::File getRoot() const { return root; }

    /** True if the file is inside the library folder. */
    bool contains (const juce::File& file) const;

    /** Re-reads the folder to pick up added, removed and changed files, then reads any missing details. */
    void rescan();

    /** Every entry: favourites first, then by name. */
    std::vector<LibraryEntry> getEntries() const;

    /** True while file details are still being read. */
    bool isReading() const { return isThreadRunning(); }

    /** Longest stretch of a file that is analysed. */
    static constexpr double maxAnalysisSeconds = 15.0 * 60.0;

    /** Reads a file in chunks and returns its tonal profile, or nothing if it can't be read or is silent. */
    static std::optional<TonalProfile> analyseFile (const juce::File& file, juce::AudioFormatManager& formats);

    /** Adds the entry to favourites, or removes it. */
    void toggleFavourite (const juce::String& key);

    /** Copies a file into the top of the library and returns the copy. A file with the same name and identical
        contents already there is reused; a different file with the same name gets a number. Safe to call from any
        thread; returns a nonexistent file if the copy fails. */
    juce::File importFile (const juce::File& source);

    /** Imports a file on the library's own import thread (which the library waits for when it is destroyed), then
        sends a change message; collect the copy with takeImportResult. Ignored while that file is already importing. */
    void importFileAsync (const juce::File& source);

    /** The library copy of a file whose background import has finished (a nonexistent file if it failed), or nothing
        while it is still running or if it was never started. Each result is returned once. */
    std::optional<juce::File> takeImportResult (const juce::File& source);

    /** Orders entries by how close they sound to a mix, closest first; entries without a profile go last, by name. */
    static void sortClosestFirst (std::vector<LibraryEntry>& entries, const TonalProfile& mix);

    /** Splits search text into lower-case words. */
    static juce::StringArray parseSearch (const juce::String& text);

    /** True if every word appears in the entry's name or folder (always true with no words). */
    static bool matchesSearch (const LibraryEntry& entry, const juce::StringArray& words);

private:
    struct Details
    {
        juce::String format;
        double seconds = 0.0;
        std::optional<TonalProfile> profile;
    };

    void run() override;
    void loadState();
    void saveCache() const;
    void saveFavourites() const;
    static juce::String cacheKey (const juce::File& file, const juce::String& key);

    const juce::File root, cacheFile, favouritesFile;
    mutable juce::CriticalSection lock;
    std::vector<LibraryEntry> entries;
    std::map<juce::String, Details> cache;
    std::set<juce::String> favourites;
    juce::CriticalSection importLock;
    std::set<juce::String> importsRunning;
    std::map<juce::String, juce::File> importResults;
    juce::ThreadPool importPool { juce::ThreadPoolOptions{}.withThreadName ("Lucky Reference import").withNumberOfThreads (1) };
};

} // namespace lucky
