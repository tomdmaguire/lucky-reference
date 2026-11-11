#pragma once

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/SlotRow.h"
#include "ui/SourceSwitch.h"

class LuckyReferenceEditor final : public juce::AudioProcessorEditor,
                                   private juce::ChangeListener,
                                   private juce::Timer
{
public:
    explicit LuckyReferenceEditor (LuckyReferenceProcessor&);
    ~LuckyReferenceEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void syncControls();
    void showMenu();
    void paintReadout (juce::Graphics&, juce::Rectangle<float> area);
    juce::Colour targetAccent() const;

    LuckyReferenceProcessor& owner;
    juce::SharedResourcePointer<lucky::LuckyLookAndFeel> lookAndFeel;
    std::array<std::unique_ptr<lucky::SlotRow>, LuckyReferenceProcessor::numSlots> rows;
    lucky::SourceSwitch sourceSwitch;
    juce::TextButton matchButton { "MATCH" }, menuButton;
    juce::ParameterAttachment referenceAttachment, slotAttachment;
    juce::ButtonParameterAttachment matchAttachment;
    bool referenceOn = false;
    int selectedSlot = 0;
    juce::Colour accent;
    float shownGainDb = 0.0f;
    bool gainShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckyReferenceEditor)
};
