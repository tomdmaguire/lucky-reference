#include "ReferenceLibrary.h"
#include "ref/FileFormat.h"

namespace lucky
{

namespace
{
constexpr int cacheVersion = 1;

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
    if (a.favourite != b.favourite)
        return a.favourite;
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
      favouritesFile (stateFolder.getChildFile ("favourites.json"))
{
    loadState();
    rescan();
}

ReferenceLibrary::~ReferenceLibrary()
{
    stopThread (5000);
}

bool ReferenceLibrary::contains (const juce::File& file) const
{
    return file.isAChildOf (root);
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
    for (const auto& item : juce::RangedDirectoryIterator (root, true, audioExtensions, juce::File::findFiles))
    {
        const auto& file = item.getFile();
        if (file.isHidden() || file.getFileName().startsWithChar ('.'))
            continue;

        LibraryEntry entry;
        entry.file = file;
        entry.key = file.getRelativePathFrom (root).replaceCharacter ('\\', '/');
        entry.name = file.getFileNameWithoutExtension();
        entry.folder = entry.key.containsChar ('/') ? entry.key.upToLastOccurrenceOf ("/", false, false) : juce::String();
        scanned.push_back (std::move (entry));
    }

    auto needsReading = false;
    {
        const juce::ScopedLock scope (lock);
        for (auto& entry : scanned)
        {
            entry.favourite = favourites.count (entry.key) > 0;
            if (const auto it = cache.find (cacheKey (entry.file, entry.key)); it != cache.end())
            {
                entry.format = it->second.format;
                entry.seconds = it->second.seconds;
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

        {
            const juce::ScopedLock scope (lock);
            cache[key] = details;
            for (auto& shown : entries)
                if (shown.key == entry.key)
                {
                    shown.format = details.format;
                    shown.seconds = details.seconds;
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

void ReferenceLibrary::toggleFavourite (const juce::String& key)
{
    {
        const juce::ScopedLock scope (lock);
        if (favourites.erase (key) == 0)
            favourites.insert (key);
        for (auto& entry : entries)
            entry.favourite = favourites.count (entry.key) > 0;
        std::sort (entries.begin(), entries.end(), libraryOrder);
    }
    saveFavourites();
    sendChangeMessage();
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
        if (target.getSize() == source.getSize())
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

    const auto savedFavourites = juce::JSON::parse (favouritesFile);
    if (const auto* list = savedFavourites.getArray())
        for (const auto& key : *list)
            favourites.insert (key.toString());

    const auto parsed = juce::JSON::parse (cacheFile);
    if (static_cast<int> (parsed["version"]) != cacheVersion)
        return;
    if (const auto* tracks = parsed["tracks"].getDynamicObject())
        for (const auto& [key, value] : tracks->getProperties())
            cache[key.toString()] = { value["format"].toString(), static_cast<double> (value["seconds"]) };
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
            tracks->setProperty (juce::Identifier (key), juce::var (track));
        }
    }

    auto* object = new juce::DynamicObject();
    object->setProperty ("version", cacheVersion);
    object->setProperty ("tracks", juce::var (tracks));
    cacheFile.getParentDirectory().createDirectory();
    cacheFile.replaceWithText (juce::JSON::toString (juce::var (object)));
}

void ReferenceLibrary::saveFavourites() const
{
    juce::Array<juce::var> list;
    {
        const juce::ScopedLock scope (lock);
        for (const auto& key : favourites)
            list.add (key);
    }
    favouritesFile.getParentDirectory().createDirectory();
    favouritesFile.replaceWithText (juce::JSON::toString (juce::var (list)));
}

} // namespace lucky
