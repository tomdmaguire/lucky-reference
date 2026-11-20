#pragma once

#include "PluginProcessor.h"
#include "ui/LibraryPanel.h"
#include "ui/RadarRipples.h"
#include "ui/SlotRow.h"
#include "ui/Theme.h"
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

    /** Adds the editor's Window size submenu to the ••• menu. */
    std::function<void (juce::PopupMenu&)> addWindowSizeMenu;

    /** Shows the LIBRARY tab over the slots, optionally sorted closest first. */
    void openLibrary (bool closest = false);

    /** Returns to the REFS tab. */
    void closeLibrary();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void syncControls();
    void showMenu();
    void chooseLibraryFolder();
    void paintReadout (juce::Graphics&, juce::Rectangle<float> area);
    void paintShortcutHint (juce::Graphics&, juce::Rectangle<float> area);
    juce::Colour targetAccent() const;
    float referenceFast() const;

    LuckyReferenceProcessor& owner;
    juce::TooltipWindow tooltips { this, 500 };
    std::array<std::unique_ptr<lucky::SlotRow>, LuckyReferenceProcessor::numSlots> rows;
    /** A transparent layer that paints through a callback, so text can sit above the ripples. */
    struct PaintLayer final : juce::Component
    {
        PaintLayer() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
        std::function<void (juce::Graphics&)> onPaint;
    };

    lucky::LibraryPanel libraryPanel;
    juce::TextButton refsTab { "REFS" }, libraryTab { "LIBRARY" };
    /** An invisible button over the logo that returns to the REFS tab. */
    juce::TextButton logoButton;
    std::unique_ptr<juce::FileChooser> folderChooser;
    juce::SharedResourcePointer<lucky::Settings> settings;
    lucky::RadarRipples radar;
    PaintLayer readout;
    lucky::SourceSwitch sourceSwitch;
    lucky::SmallSwitch matchSwitch { "LEVEL MATCH" };
    juce::DrawableButton menuButton { "Menu", juce::DrawableButton::ImageFitted };
    juce::ParameterAttachment referenceAttachment, slotAttachment, matchAttachment;
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

class LuckyReferenceEditor final : public lucky::ScaledEditor
{
public:
    explicit LuckyReferenceEditor (LuckyReferenceProcessor&);

    /** Opens the LIBRARY tab, optionally sorted closest first. */
    void showLibrary (bool closest = false) { view.openLibrary (closest); }

private:
    LuckyReferenceView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckyReferenceEditor)
};
