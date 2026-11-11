#pragma once

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/LoudnessRings.h"
#include "ui/Settings.h"
#include "ui/SlotRow.h"
#include "ui/SourceSwitch.h"

/** The editor's content at its base size; the editor scales it to the window. */
class LuckyReferenceView final : public juce::Component,
                                 private juce::ChangeListener,
                                 private juce::Timer
{
public:
    static constexpr int baseWidth = 900;
    static constexpr int baseHeight = 600;

    explicit LuckyReferenceView (LuckyReferenceProcessor&);
    ~LuckyReferenceView() override;

    /** Asks the editor to resize to a scale of the base size. */
    std::function<void (float scale)> onScaleRequested;

    /** The editor's current scale, for ticking the Window size menu. */
    std::function<float()> getScale;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void syncControls();
    void showMenu();
    void paintReadout (juce::Graphics&, juce::Rectangle<float> area);
    juce::Colour targetAccent() const;
    float referenceMomentary() const;

    LuckyReferenceProcessor& owner;
    std::array<std::unique_ptr<lucky::SlotRow>, LuckyReferenceProcessor::numSlots> rows;
    lucky::LoudnessRings rings;
    lucky::SourceSwitch sourceSwitch;
    juce::TextButton matchButton { "LEVEL MATCH" };
    juce::DrawableButton menuButton { "Menu", juce::DrawableButton::ImageFitted };
    juce::ParameterAttachment referenceAttachment, slotAttachment;
    juce::ButtonParameterAttachment matchAttachment;
    bool referenceOn = false;
    int selectedSlot = 0;
    bool controlsSynced = false;
    bool focusTaken = false;
    bool sourceSwitchShowsReference = false;
    int shownSlot = 0;
    juce::Colour accent;
    float shownGainDb = 0.0f;
    bool gainShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckyReferenceView)
};

class LuckyReferenceEditor final : public juce::AudioProcessorEditor
{
public:
    explicit LuckyReferenceEditor (LuckyReferenceProcessor&);

    void resized() override;

private:
    float currentScale() const;

    juce::SharedResourcePointer<lucky::LuckyLookAndFeel> lookAndFeel;
    juce::SharedResourcePointer<lucky::Settings> settings;
    LuckyReferenceView view;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckyReferenceEditor)
};
