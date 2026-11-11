#include "library/ReferenceLibrary.h"
#include "TestSignals.h"

namespace lucky
{

namespace
{
constexpr double rate = 48000.0;

bool waitUntilRead (const ReferenceLibrary& library)
{
    for (int attempt = 0; attempt < 500 && library.isReading(); ++attempt)
        juce::Thread::sleep (10);
    return ! library.isReading();
}

const LibraryEntry* findEntry (const std::vector<LibraryEntry>& entries, const juce::String& key)
{
    for (const auto& entry : entries)
        if (entry.key == key)
            return &entry;
    return nullptr;
}

juce::StringArray keysOf (const std::vector<LibraryEntry>& entries)
{
    juce::StringArray keys;
    for (const auto& entry : entries)
        keys.add (entry.key);
    return keys;
}
} // namespace

class LibraryTests final : public juce::UnitTest
{
public:
    LibraryTests() : juce::UnitTest ("Reference library", "Lucky Reference") {}

    void runTest() override
    {
        const auto folder = test::scratchFolder ("library");
        const auto root = folder.getChildFile ("References");
        const auto state = folder.getChildFile ("State");
        root.getChildFile ("Club").createDirectory();
        test::writeWav (root.getChildFile ("Bravo.wav"), test::sine (-12.0f, 2.0, rate), rate);
        test::writeWav (root.getChildFile ("alpha.wav"), test::sine (-12.0f, 1.0, rate), rate);
        test::writeWav (root.getChildFile ("Club").getChildFile ("Charlie Night.wav"), test::sine (-12.0f, 3.0, rate), rate);
        root.getChildFile ("notes.txt").replaceWithText ("not audio");
        root.getChildFile (".hidden.wav").replaceWithText ("hidden");

        beginTest ("Scans audio files in subfolders, sorted by name, and reads their details");
        {
            ReferenceLibrary library (root, state);
            expect (waitUntilRead (library));
            const auto entries = library.getEntries();
            expectEquals (keysOf (entries).joinIntoString (","), juce::String ("alpha.wav,Bravo.wav,Club/Charlie Night.wav"));

            const auto* charlie = findEntry (entries, "Club/Charlie Night.wav");
            expect (charlie != nullptr);
            expectEquals (charlie->name, juce::String ("Charlie Night"));
            expectEquals (charlie->folder, juce::String ("Club"));
            expectEquals (charlie->format, juce::String ("WAV"));
            expectWithinAbsoluteError (charlie->seconds, 3.0, 0.01);
            expect (library.contains (charlie->file));
            expectEquals (findEntry (entries, "alpha.wav")->folder, juce::String(), "top-level tracks have no folder");
            expect (charlie->analysed);
            expect (charlie->profile.has_value(), "an audible track gets a tonal profile");
            expect (! library.contains (folder.getChildFile ("elsewhere.wav")));
        }

        beginTest ("Cached details are reused, and a changed file is read again");
        {
            {
                ReferenceLibrary library (root, state);
                expect (! library.isReading(), "nothing new to read");
                const auto entries = library.getEntries();
                const auto* bravo = findEntry (entries, "Bravo.wav");
                expectWithinAbsoluteError (bravo->seconds, 2.0, 0.01);
                expect (bravo->analysed && bravo->profile.has_value(), "the profile comes from the cache");

                juce::AudioFormatManager formats;
                formats.registerBasicFormats();
                const auto fresh = ReferenceLibrary::analyseFile (bravo->file, formats);
                expect (fresh.has_value());
                expectLessThan (TonalProfile::distance (*fresh, *bravo->profile), 0.05f, "the cached profile matches a fresh analysis");
            }

            test::writeWav (root.getChildFile ("Bravo.wav"), test::sine (-12.0f, 4.0, rate), rate);
            root.getChildFile ("Bravo.wav").setLastModificationTime (juce::Time::getCurrentTime() + juce::RelativeTime::seconds (5.0));
            ReferenceLibrary library (root, state);
            expect (waitUntilRead (library));
            expectWithinAbsoluteError (findEntry (library.getEntries(), "Bravo.wav")->seconds, 4.0, 0.01);
        }

        beginTest ("A rescan drops deleted files and finds new ones");
        {
            ReferenceLibrary library (root, state);
            root.getChildFile ("alpha.wav").deleteFile();
            test::writeWav (root.getChildFile ("Delta.wav"), test::sine (-12.0f, 1.0, rate), rate);
            library.rescan();
            expect (waitUntilRead (library));
            expectEquals (keysOf (library.getEntries()).joinIntoString (","), juce::String ("Bravo.wav,Club/Charlie Night.wav,Delta.wav"));
        }

        beginTest ("Favourites sort first and are remembered");
        {
            {
                ReferenceLibrary library (root, state);
                library.toggleFavourite ("Delta.wav");
                const auto entries = library.getEntries();
                expectEquals (entries.front().key, juce::String ("Delta.wav"));
                expect (entries.front().favourite);
            }

            ReferenceLibrary library (root, state);
            expectEquals (library.getEntries().front().key, juce::String ("Delta.wav"));
            library.toggleFavourite ("Delta.wav");
            expect (! findEntry (library.getEntries(), "Delta.wav")->favourite);
        }

        beginTest ("Search needs every word in the name or folder, ignoring case");
        {
            ReferenceLibrary library (root, state);
            const auto matches = [&] (const juce::String& text)
            {
                juce::StringArray keys;
                for (const auto& entry : library.getEntries())
                    if (ReferenceLibrary::matchesSearch (entry, ReferenceLibrary::parseSearch (text)))
                        keys.add (entry.key);
                return keys.joinIntoString (",");
            };

            expectEquals (matches ("club"), juce::String ("Club/Charlie Night.wav"));
            expectEquals (matches ("NIGHT charlie"), juce::String ("Club/Charlie Night.wav"));
            expectEquals (matches ("charlie bravo"), juce::String());
            expectEquals (matches ("  "), juce::String ("Bravo.wav,Club/Charlie Night.wav,Delta.wav"));
        }

        beginTest ("Importing copies a file in once, and numbers a different file with the same name");
        {
            ReferenceLibrary library (root, state);
            const auto outside = folder.getChildFile ("Outside");
            outside.createDirectory();
            const auto source = outside.getChildFile ("Echo.wav");
            test::writeWav (source, test::sine (-12.0f, 1.0, rate), rate);

            const auto copy = library.importFile (source);
            expectEquals (copy.getFullPathName(), root.getChildFile ("Echo.wav").getFullPathName());
            expectEquals (copy.getSize(), source.getSize());
            expect (source.existsAsFile(), "the original stays where it was");

            expectEquals (library.importFile (source).getFullPathName(), copy.getFullPathName());
            expectEquals (library.importFile (copy).getFullPathName(), copy.getFullPathName());

            const auto other = folder.getChildFile ("Other");
            other.createDirectory();
            test::writeWav (other.getChildFile ("Echo.wav"), test::sine (-12.0f, 2.0, rate), rate);
            expectEquals (library.importFile (other.getChildFile ("Echo.wav")).getFileName(), juce::String ("Echo 2.wav"));
            expect (library.importFile (folder.getChildFile ("missing.wav")) == juce::File());

            const auto sameSize = folder.getChildFile ("SameSize");
            sameSize.createDirectory();
            test::writeWav (sameSize.getChildFile ("Echo.wav"), test::sine (-12.0f, 1.0, rate, 2, 440.0), rate);
            expectEquals (sameSize.getChildFile ("Echo.wav").getSize(), source.getSize(), "the test needs a file of the same size");
            expectEquals (library.importFile (sameSize.getChildFile ("Echo.wav")).getFileName(), juce::String ("Echo 3.wav"),
                          "a different file with the same name and size is not mistaken for the copy");
        }

        beginTest ("A background import reports its copy once");
        {
            ReferenceLibrary library (root, state);
            const auto source = folder.getChildFile ("Outside").getChildFile ("Foxtrot.wav");
            test::writeWav (source, test::sine (-12.0f, 1.0, rate), rate);
            expect (! library.takeImportResult (source).has_value());
            library.importFileAsync (source);
            std::optional<juce::File> copy;
            for (int attempt = 0; attempt < 500 && ! copy; ++attempt)
            {
                juce::Thread::sleep (10);
                copy = library.takeImportResult (source);
            }
            expect (copy.has_value() && copy->existsAsFile());
            expectEquals (copy->getFileName(), juce::String ("Foxtrot.wav"));
            expect (! library.takeImportResult (source).has_value(), "each result is returned once");
        }
    }
};

static LibraryTests libraryTests;

} // namespace lucky
