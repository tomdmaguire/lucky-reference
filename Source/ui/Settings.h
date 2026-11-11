#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace lucky
{

/** Settings shared by every Lucky Reference instance, stored in ~/Library/Application Support/Lucky Reference. */
class Settings
{
public:
    Settings()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Lucky Reference";
        options.filenameSuffix = ".settings";
        options.folderName = "Lucky Reference";
        options.osxLibrarySubFolder = "Application Support";
        options.millisecondsBeforeSaving = 1000;
        properties.setStorageParameters (options);
    }

    /** The editor scale last chosen, 1.0 by default. */
    float getUiScale() const { return static_cast<float> (properties.getUserSettings()->getDoubleValue ("uiScale", 1.0)); }

    /** The folder a reference was last chosen or dropped from, or a nonexistent file when there isn't one. */
    juce::File getLastReferenceFolder() const { return juce::File (properties.getUserSettings()->getValue ("lastReferenceFolder")); }

    /** Remembers where references come from, so browsing starts there next time. */
    void setLastReferenceFolder (const juce::File& folder) { properties.getUserSettings()->setValue ("lastReferenceFolder", folder.getFullPathName()); }

    /** Remembers the editor scale for new windows. */
    void setUiScale (float scale)
    {
        if (! juce::approximatelyEqual (getUiScale(), scale))
            properties.getUserSettings()->setValue ("uiScale", scale);
    }

private:
    mutable juce::ApplicationProperties properties;
};

} // namespace lucky
