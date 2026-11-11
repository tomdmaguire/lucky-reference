#pragma once

#include "library/ReferenceLibrary.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** The reference library, shown over the slot rows: a search box, the library's tracks sorted by name or by how close
    they sound to the mix, and on every row a 1 2 3 button per slot to load the track into. Files added here, by
    ADD FILES or by dropping them on the panel, are copied into the library. */
class LibraryPanel final : public juce::Component,
                           public juce::FileDragAndDropTarget,
                           private juce::ListBoxModel,
                           private juce::ChangeListener,
                           private juce::KeyListener,
                           private juce::Timer
{
public:
    static constexpr int numSlots = 3;

    explicit LibraryPanel (ReferenceLibrary& library);
    ~LibraryPanel() override;

    /** Called to load a library track into a slot (0-2). */
    std::function<void (int slot, const juce::File&)> onChoose;

    /** Returns the file loaded in a slot, so each row can show which slots hold it. */
    std::function<juce::File (int slot)> slotFile;

    /** Called when Escape is pressed with an empty search, to leave the library. */
    std::function<void()> onClose;

    /** The mix's tonal profile once enough has been heard, for sorting by closest match. */
    std::function<std::optional<TonalProfile>()> mixProfile;

    /** Seconds of the mix heard so far, for the "play your mix" message. */
    std::function<double()> mixSeconds;

    /** Seconds of the mix needed before it can be compared. */
    double mixSecondsNeeded = 20.0;

    /** Rescans the library and takes keyboard focus for the list keys (the search box only takes it when clicked, so
        keys the panel doesn't use, such as the host's space bar, still reach the host). */
    void opened();

    /** Stops the periodic rescan. */
    void closed();

    /** Sorts by closest match to the mix (true) or by name (false). */
    void setClosest (bool closest);

    /** Redraws the slot buttons after the slots change. */
    void slotsChanged() { list.repaint(); }

    /** Copies files into the library in the background, then rescans. */
    void addFiles (const juce::Array<juce::File>& files);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int lastRowSelected) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress&, juce::Component* originator) override;
    void timerCallback() override;

    /** Ranks against the mix as it is now, or clears the ranking if the mix isn't ready. */
    void rank();
    void refresh();
    void updateVisibility();
    bool waitingForMix() const;
    juce::String hintText() const;
    void chooseRow (int row, int slot);
    /** Loads a row into the first empty slot, or explains that every slot is full. */
    void chooseRowIntoEmptySlot (int row);
    void showNotice (const juce::String& text);
    void browseForFiles();
    juce::Rectangle<float> slotButtonArea (int slot, int width, int height) const;
    int slotButtonAt (juce::Point<int> positionInRow, int width, int height) const;

    ReferenceLibrary& library;
    std::vector<LibraryEntry> shown;
    int totalEntries = 0, analysedEntries = 0;
    int ticks = 0;
    bool sortClosest = false;
    /** The mix profile the list was last ranked against; kept fixed so rows don't move while you browse. */
    std::optional<TonalProfile> rankedAgainst;
    double rankedSeconds = 0.0;
    int hoverRow = -1, hoverSlot = -1;
    juce::String notice;
    juce::uint32 noticeUntil = 0;
    juce::Array<juce::File> importing;
    juce::TextEditor searchBox;
    juce::TextButton clearSearchButton, addButton { "ADD FILES..." }, finderButton { "SHOW IN FINDER" };
    juce::TextButton nameOrderButton { "A-Z" }, closestOrderButton { "CLOSEST" };
    juce::ListBox list { "Library", this };
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace lucky
