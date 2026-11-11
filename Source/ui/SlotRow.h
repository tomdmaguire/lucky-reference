#pragma once

#include "PluginProcessor.h"
#include "Settings.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

namespace lucky
{

/** One reference slot: its waveform with the loop and playhead, a drop target for files, and loop editing by dragging. */
class SlotRow final : public juce::Component,
                      public juce::FileDragAndDropTarget
{
public:
    SlotRow (LuckyReferenceProcessor& processor, int slot);

    /** Called when the row is clicked, to make it the selected slot. */
    std::function<void()> onSelect;

    /** Called to choose this slot's reference from the library (clicking an empty slot, or the menu). */
    std::function<void()> onOpenLibrary;

    /** Marks the row as the selected slot and whether it's the one being heard. */
    void setState (bool isSelected, bool isAudible);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    /** Opens a file browser for the slot. */
    void browse();

    static inline const juce::String audioExtensions { "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.aac;*.caf" };

private:
    enum class Drag { none, newLoop, startEdge, endEdge, blocked };

    /** This slot's folder, else another slot's (usually the project's), else the last folder used, else Music. */
    juce::File browseStartFolder() const;
    void loadAndRemember (const juce::File& file);

    juce::Rectangle<float> waveformArea() const;
    double secondsAt (float x) const;
    float xAt (double seconds) const;
    Drag edgeAt (float x) const;
    LoopRegion shownLoop() const;
    void showMenu();
    /** True when the slot's file is loaded and lives outside the library, so it can be added. */
    bool canAddToLibrary() const;
    void addToLibrary();

    LuckyReferenceProcessor& processor;
    const int slot;
    bool selected = false, audible = false, dropHover = false;
    Drag drag = Drag::none;
    double dragAnchorSeconds = 0.0;
    std::optional<LoopRegion> draft;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<float> addButtonArea;
    bool addHover = false, adding = false;
    juce::SharedResourcePointer<Settings> settings;
};

} // namespace lucky
