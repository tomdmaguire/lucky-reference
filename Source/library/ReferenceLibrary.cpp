#include "ReferenceLibrary.h"
#include "ref/FileFormat.h"

namespace lucky
{

namespace
{
constexpr int cacheVersion = 2;
constexpr int analysisChunk = 1 << 16;

juce::var profileToVar (const TonalProfile& profile)
{
    juce::Array<juce::var> bands;
    for (const auto level : profile.bandsDb)
        bands.add (std::round (level * 100.0f) / 100.0f);
    auto* object = new juce::DynamicObject();
    object->setProperty ("bands", bands);
    object->setProperty ("punch", std::round (profile.punchDb * 100.0f) / 100.0f);
    return juce::var (object);
}

std::optional<TonalProfile> profileFromVar (const juce::var& value)
{
    const auto* bands = value["bands"].getArray();
    if (bands == nullptr || bands->size() != TonalProfile::numBands)
        return std::nullopt;
    TonalProfile profile;
    for (int i = 0; i < TonalProfile::numBands; ++i)
        profile.bandsDb[static_cast<size_t> (i)] = static_cast<float> ((*bands)[i]);
    profile.punchDb = static_cast<float> (value["punch"]);
    return profile;
}

juce::File folderFromEnvironment (const char* name, const juce::File& fallback)
{
    const auto value = juce::SystemStats::getEnvironmentVariable (name, {});
    return value.isNotEmpty() ? juce::File (value) : fallback;
}

juce::File defaultRoot()
{
    return folderFromEnvironment ("LUCKY_REFERENCE_LIBRARY",
                                  juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Lucky Reference").getChildFile ("References"));
}

juce::File defaultStateFolder()
{
    return folderFromEnvironment ("LUCKY_REFERENCE_STATE", juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                                               .getChildFile ("Application Support")
                                                               .getChildFile ("Lucky Reference"));
}

bool libraryOrder (const LibraryEntry& a, const LibraryEntry& b)
{
    if (const auto byName = a.name.compareNatural (b.name); byName != 0)
        return byName < 0;
    return a.key.compareNatural (b.key) < 0;
}
} // namespace

ReferenceLibrary::ReferenceLibrary() : ReferenceLibrary (defaultRoot(), defaultStateFolder()) {}

ReferenceLibrary::ReferenceLibrary (const juce::File& libraryRoot, const juce::File& stateFolder)
    : juce::Thread ("Lucky Reference library"),
      root (libraryRoot),
      cacheFile (stateFolder.getChildFile ("library-cache.json")),
      foldersFile (stateFolder.getChildFile ("folders.json"))
{
    loadState();
    rescan();
}

ReferenceLibrary::~ReferenceLibrary()
{
    importPool.removeAllJobs (true, 30000);
    stopThread (5000);
}

bool ReferenceLibrary::contains (const juce::File& file) const
{
    if (file.isAChildOf (root))
        return true;
    for (const auto& folder : getFolders())
        if (file.isAChildOf (folder))
            return true;
    return false;
}

juce::Array<juce::File> ReferenceLibrary::getFolders() const
{
    const juce::ScopedLock scope (lock);
    return folders;
}

void ReferenceLibrary::addFolder (const juce::File& folder)
{
    {
        const juce::ScopedLock scope (lock);
        if (! folder.isDirectory() || folder == root || folder.isAChildOf (root) || folders.contains (folder))
            return;
        for (const auto& existing : folders)
            if (folder.isAChildOf (existing))
                return;
        folders.removeIf ([&folder] (const juce::File& existing) { return existing.isAChildOf (folder); });
        folders.add (folder);
    }
    saveFolders();
    rescan();
}

void ReferenceLibrary::removeFolder (const juce::File& folder)
{
    {
        const juce::ScopedLock scope (lock);
        folders.removeAllInstancesOf (folder);
    }
    saveFolders();
    rescan();
}

juce::String ReferenceLibrary::cacheKey (const juce::File& file, const juce::String& key)
{
    return key + "|" + juce::String (file.getSize()) + "|" + juce::String (file.getLastModificationTime().toMilliseconds());
}

void ReferenceLibrary::rescan()
{
    stopThread (5000);
    root.createDirectory();

    std::vector<LibraryEntry> scanned;
    const auto scan = [this, &scanned] (const juce::File& folder, bool isMain)
    {
        if (! folder.isDirectory())
            return;
        for (const auto& item : juce::RangedDirectoryIterator (folder, true, audioExtensions, juce::File::findFiles))
        {
            const auto& file = item.getFile();
            if (file.isHidden() || file.getFileName().startsWithChar ('.') || (! isMain && file.isAChildOf (root)))
                continue;

            const auto relative = file.getRelativePathFrom (folder).replaceCharacter ('\\', '/');
            const auto subfolder = relative.containsChar ('/') ? relative.upToLastOccurrenceOf ("/", false, false) : juce::String();
            LibraryEntry entry;
            entry.file = file;
            entry.key = isMain ? relative : file.getFullPathName();
            entry.name = file.getFileNameWithoutExtension();
            entry.folder = isMain ? subfolder : folder.getFileName() + (subfolder.isEmpty() ? juce::String() : "/" + subfolder);
            scanned.push_back (std::move (entry));
        }
    };
    scan (root, true);
    for (const auto& folder : getFolders())
        scan (folder, false);

    auto needsReading = false;
    {
        const juce::ScopedLock scope (lock);
        for (auto& entry : scanned)
        {
            if (const auto it = cache.find (cacheKey (entry.file, entry.key)); it != cache.end())
            {
                entry.format = it->second.format;
                entry.seconds = it->second.seconds;
                entry.profile = it->second.profile;
                entry.analysed = true;
            }
            else
            {
                needsReading = true;
            }
        }
        std::sort (scanned.begin(), scanned.end(), libraryOrder);
        entries = std::move (scanned);
    }

    sendChangeMessage();
    if (needsReading)
        startThread (juce::Thread::Priority::low);
}

void ReferenceLibrary::run()
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::vector<LibraryEntry> snapshot;
    {
        const juce::ScopedLock scope (lock);
        snapshot = entries;
    }

    auto cacheChanged = false;
    auto lastNotice = juce::Time::getMillisecondCounter();
    for (const auto& entry : snapshot)
    {
        if (threadShouldExit())
            break;

        const auto key = cacheKey (entry.file, entry.key);
        {
            const juce::ScopedLock scope (lock);
            if (cache.count (key) > 0)
                continue;
        }

        Details details;
        details.format = describeFileFormat (entry.file);
        if (std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (entry.file)); reader != nullptr && reader->sampleRate > 0.0)
            details.seconds = static_cast<double> (reader->lengthInSamples) / reader->sampleRate;
        details.profile = analyseFile (entry.file, formats);
        if (threadShouldExit())
            break;

        {
            const juce::ScopedLock scope (lock);
            cache[key] = details;
            for (auto& shown : entries)
                if (shown.key == entry.key)
                {
                    shown.format = details.format;
                    shown.seconds = details.seconds;
                    shown.profile = details.profile;
                    shown.analysed = true;
                }
        }
        cacheChanged = true;

        if (juce::Time::getMillisecondCounter() - lastNotice > 250)
        {
            lastNotice = juce::Time::getMillisecondCounter();
            sendChangeMessage();
        }
    }

    if (cacheChanged)
        saveCache();
    sendChangeMessage();
}

std::vector<LibraryEntry> ReferenceLibrary::getEntries() const
{
    const juce::ScopedLock scope (lock);
    return entries;
}

void ReferenceLibrary::importFileAsync (const juce::File& source)
{
    const auto key = source.getFullPathName();
    {
        const juce::ScopedLock scope (lock);
        if (! importsRunning.insert (key).second)
            return;
        importResults.erase (key);
    }

    importPool.addJob ([this, source, key]
    {
        const auto copy = importFile (source);
        {
            const juce::ScopedLock scope (lock);
            importsRunning.erase (key);
            importResults[key] = copy;
        }
        sendChangeMessage();
    });
}

std::optional<juce::File> ReferenceLibrary::takeImportResult (const juce::File& source)
{
    const juce::ScopedLock scope (lock);
    const auto it = importResults.find (source.getFullPathName());
    if (it == importResults.end())
        return std::nullopt;
    const auto copy = it->second;
    importResults.erase (it);
    return copy;
}

juce::File ReferenceLibrary::importFile (const juce::File& source)
{
    if (! source.existsAsFile())
        return {};
    if (contains (source))
        return source;

    const juce::ScopedLock scope (importLock);
    root.createDirectory();
    auto target = root.getChildFile (source.getFileName());
    for (int number = 2; target.exists(); ++number)
    {
        if (target.getSize() == source.getSize() && target.hasIdenticalContentTo (source))
            return target;
        target = root.getChildFile (source.getFileNameWithoutExtension() + " " + juce::String (number) + source.getFileExtension());
    }

    const auto partial = target.getSiblingFile ("." + target.getFileName() + ".partial");
    if (! source.copyFileTo (partial) || ! partial.moveFileTo (target))
    {
        partial.deleteFile();
        return {};
    }
    return target;
}

std::optional<TonalProfile> ReferenceLibrary::analyseFile (const juce::File& file, juce::AudioFormatManager& formats)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->numChannels == 0)
        return std::nullopt;

    TonalAnalyser analyser (reader->sampleRate);
    const auto channels = static_cast<int> (std::min (2u, reader->numChannels));
    const auto length = std::min (reader->lengthInSamples, static_cast<juce::int64> (maxAnalysisSeconds * reader->sampleRate));
    juce::AudioBuffer<float> chunk (channels, analysisChunk);
    std::vector<float> mono (analysisChunk);
    for (juce::int64 start = 0; start < length; start += analysisChunk)
    {
        if (juce::Thread::currentThreadShouldExit())
            return std::nullopt;
        const auto count = static_cast<int> (std::min<juce::int64> (analysisChunk, length - start));
        if (! reader->read (&chunk, 0, count, start, true, channels > 1))
            return std::nullopt;
        for (int i = 0; i < count; ++i)
            mono[static_cast<size_t> (i)] = channels > 1 ? 0.5f * (chunk.getSample (0, i) + chunk.getSample (1, i)) : chunk.getSample (0, i);
        analyser.process (mono.data(), count);
    }
    return analyser.getProfile();
}

void ReferenceLibrary::sortClosestFirst (std::vector<LibraryEntry>& entries, const TonalProfile& mix)
{
    std::stable_sort (entries.begin(), entries.end(), [&mix] (const LibraryEntry& a, const LibraryEntry& b)
                      {
                          if (a.profile.has_value() != b.profile.has_value())
                              return a.profile.has_value();
                          if (! a.profile)
                              return a.name.compareNatural (b.name) < 0;
                          return TonalProfile::distance (*a.profile, mix) < TonalProfile::distance (*b.profile, mix);
                      });
}

juce::StringArray ReferenceLibrary::parseSearch (const juce::String& text)
{
    auto words = juce::StringArray::fromTokens (text.toLowerCase(), true);
    words.removeEmptyStrings();
    return words;
}

bool ReferenceLibrary::matchesSearch (const LibraryEntry& entry, const juce::StringArray& words)
{
    const auto haystack = (entry.name + " " + entry.folder).toLowerCase();
    for (const auto& word : words)
        if (! haystack.contains (word))
            return false;
    return true;
}

void ReferenceLibrary::loadState()
{
    const juce::ScopedLock scope (lock);

    const auto savedFolders = juce::JSON::parse (foldersFile);
    if (const auto* list = savedFolders.getArray())
        for (const auto& path : *list)
            if (const auto folder = path.toString(); juce::File::isAbsolutePath (folder))
                folders.addIfNotAlreadyThere (juce::File (folder));

    const auto parsed = juce::JSON::parse (cacheFile);
    if (static_cast<int> (parsed["version"]) != cacheVersion)
        return;
    if (const auto* tracks = parsed["tracks"].getDynamicObject())
        for (const auto& [key, value] : tracks->getProperties())
            cache[key.toString()] = { value["format"].toString(), static_cast<double> (value["seconds"]), profileFromVar (value["profile"]) };
}

void ReferenceLibrary::saveCache() const
{
    auto* tracks = new juce::DynamicObject();
    {
        const juce::ScopedLock scope (lock);
        for (const auto& [key, details] : cache)
        {
            auto* track = new juce::DynamicObject();
            track->setProperty ("format", details.format);
            track->setProperty ("seconds", details.seconds);
            if (details.profile)
                track->setProperty ("profile", profileToVar (*details.profile));
            tracks->setProperty (juce::Identifier (key), juce::var (track));
        }
    }

    auto* object = new juce::DynamicObject();
    object->setProperty ("version", cacheVersion);
    object->setProperty ("tracks", juce::var (tracks));
    cacheFile.getParentDirectory().createDirectory();
    cacheFile.replaceWithText (juce::JSON::toString (juce::var (object)));
}

void ReferenceLibrary::saveFolders() const
{
    juce::Array<juce::var> list;
    {
        const juce::ScopedLock scope (lock);
        for (const auto& folder : folders)
            list.add (folder.getFullPathName());
    }
    foldersFile.getParentDirectory().createDirectory();
    foldersFile.replaceWithText (juce::JSON::toString (juce::var (list)));
}

} // namespace lucky
