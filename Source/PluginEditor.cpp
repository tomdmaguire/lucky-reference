#include "PluginEditor.h"
#include "ui/Theme.h"

LuckyReferenceEditor::LuckyReferenceEditor (LuckyReferenceProcessor& owner)
    : AudioProcessorEditor (owner)
{
    setSize (720, 360);
}

void LuckyReferenceEditor::paint (juce::Graphics& g)
{
    g.fillAll (lucky::theme::background);
}
