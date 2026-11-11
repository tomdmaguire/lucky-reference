#pragma once

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"

class LuckyReferenceEditor final : public juce::AudioProcessorEditor
{
public:
    explicit LuckyReferenceEditor (LuckyReferenceProcessor&);

    void paint (juce::Graphics&) override;

private:
    juce::SharedResourcePointer<lucky::LuckyLookAndFeel> lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckyReferenceEditor)
};
