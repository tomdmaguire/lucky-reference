#pragma once

#include "library/ReferenceLibrary.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** The reference library, shown over the slot rows: a search box, the library's tracks (favourites first, with a
    heart to toggle), and buttons to browse for a file instead or close. Choosing a track loads it into the target slot. */
class LibraryPanel final : public juce::Component,
                           public juce::FileDragAndDropTarget,
                           private juce::ListBoxModel,
                           private juce::ChangeListener,
                           private juce::KeyListener,
                           private juce::Timer
{
public:
    explicit LibraryPanel (ReferenceLibrary& library);
    ~LibraryPanel() override;

    /** Called with the chosen file, from a double-click, Return or a file dropped on the panel. */
    std::function<void (const juce::File&)> onChoose;

    /** Called when "Browse files..." is clicked. */
    std::function<void()> onBrowse;

    /** Called when the panel asks to close (close button or Escape). */
    std::function<void()> onClose;

    /** Returns the slot (0-2) a file is loaded in, or -1, so the list can mark loaded tracks. */
    std::function<int (const juce::File&)> slotOf;

    /** The slot a chosen track goes into, shown in the header in that slot's colour. */
    void setTarget (int slot);

    /** Rescans the library, highlights the target slot's current track (else the first), selects the search text and
        focuses the search box. */
    void opened (const juce::File& current);

    /** Stops the periodic rescan. */
    void closed();

    /** Redraws the markers showing which tracks are loaded, after the slots change. */
    void slotsChanged() { list.repaint(); }

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

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

    void refresh();
    void chooseRow (int row);
    void showEmptyState();
    juce::Rectangle<int> heartArea (int width, int height) const;

    ReferenceLibrary& library;
    std::vector<LibraryEntry> shown;
    int totalEntries = 0;
    int target = 0;
    juce::TextEditor searchBox;
    juce::TextButton clearSearchButton, browseButton { "BROWSE FILES..." }, finderButton { "SHOW IN FINDER" }, closeButton;
    juce::ListBox list { "Library", this };
};

} // namespace lucky
