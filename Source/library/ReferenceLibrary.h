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
    /** Identifies the entry: its path inside the main folder with forward slashes (e.g. "Club/Track.wav"), or its full
        path for a track in an added folder. */
    juce::String key;
    /** File name without its extension. */
    juce::String name;
    /** Where the track sits: its subfolder in the main folder (e.g. "Club"), empty at the top level, or an added
        folder's name followed by any subfolder (e.g. "Album Refs/Disc 1"). */
    juce::String folder;
    /** Badge text from describeFileFormat, empty until the details are read. */
    juce::String format;
    /** Length in seconds, 0 until the details are read. */
    double seconds = 0.0;
    /** How the track sounds, once analysed; stays empty for silent or unreadable files. */
    std::optional<TonalProfile> profile;
    /** True once the file's details and profile have been read. */
    bool analysed = false;
};

/** The reference library: a main folder of reference tracks plus any folders the user adds, shared by every plugin
    instance. A rescan lists the folders (including subfolders) on the calling thread, then a low-priority thread reads
    each new or changed file's details, which are cached so they are only read once. */
class ReferenceLibrary final : public juce::ChangeBroadcaster,
                               private juce::Thread
{
public:
    /** The library in ~/Music/Lucky Reference/References (or $LUCKY_REFERENCE_LIBRARY), with its cache and added
        folders in ~/Library/Application Support/Lucky Reference (or $LUCKY_REFERENCE_STATE). */
    ReferenceLibrary();

    /** A library in any folder, keeping its cache and added folders in stateFolder. */
    ReferenceLibrary (const juce::File& root, const juce::File& stateFolder);

    ~ReferenceLibrary() override;

    static inline const juce::String audioExtensions { "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.aac;*.caf" };

    /** The main library folder, created if it doesn't exist; imports are copied here. */
    juce::File getRoot() const { return root; }

    /** Folders added with addFolder, which are indexed where they are. */
    juce::Array<juce::File> getFolders() const;

    /** Adds a folder to the library (ignored if it is already covered) and rescans. */
    void addFolder (const juce::File& folder);

    /** Removes an added folder from the library (its files stay where they are) and rescans. */
    void removeFolder (const juce::File& folder);

    /** True if the file is inside the main folder or an added folder. */
    bool contains (const juce::File& file) const;

    /** Re-reads the folder to pick up added, removed and changed files, then reads any missing details. */
    void rescan();

    /** Every entry, by name. */
    std::vector<LibraryEntry> getEntries() const;

    /** True while file details are still being read. */
    bool isReading() const { return isThreadRunning(); }

    /** Longest stretch of a file that is analysed. */
    static constexpr double maxAnalysisSeconds = 15.0 * 60.0;

    /** Reads a file in chunks and returns its tonal profile, or nothing if it can't be read or is silent. */
    static std::optional<TonalProfile> analyseFile (const juce::File& file, juce::AudioFormatManager& formats);

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
    void saveFolders() const;
    static juce::String cacheKey (const juce::File& file, const juce::String& key);

    const juce::File root, cacheFile, foldersFile;
    mutable juce::CriticalSection lock;
    std::vector<LibraryEntry> entries;
    std::map<juce::String, Details> cache;
    juce::Array<juce::File> folders;
    juce::CriticalSection importLock;
    std::set<juce::String> importsRunning;
    std::map<juce::String, juce::File> importResults;
    juce::ThreadPool importPool { juce::ThreadPoolOptions{}.withThreadName ("Lucky Reference import").withNumberOfThreads (1) };
};

} // namespace lucky
