#include "LibraryPanel.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr int headerHeight = 52;
constexpr int controlHeight = 30;
constexpr int padding = 12;
constexpr int rowHeight = 30;
constexpr int rescanMilliseconds = 5000;

juce::String formatDuration (double seconds)
{
    if (seconds <= 0.0)
        return {};
    const auto total = juce::roundToInt (seconds);
    return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}
} // namespace

LibraryPanel::LibraryPanel (ReferenceLibrary& referenceLibrary) : library (referenceLibrary)
{
    searchBox.setTextToShowWhenEmpty ("Search your library", theme::textDim);
    searchBox.setFont (juce::FontOptions (14.0f));
    searchBox.setIndents (30, 7);
    searchBox.setColour (juce::TextEditor::backgroundColourId, theme::topBar);
    searchBox.setColour (juce::TextEditor::textColourId, theme::text);
    searchBox.setColour (juce::TextEditor::outlineColourId, theme::outline);
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, theme::textDim);
    searchBox.setColour (juce::TextEditor::highlightColourId, theme::text.withAlpha (0.2f));
    searchBox.setColour (juce::CaretComponent::caretColourId, theme::text);
    searchBox.onTextChange = [this] { refresh(); };
    searchBox.onReturnKey = [this] { chooseRow (list.getSelectedRow()); };
    searchBox.onEscapeKey = [this]
    {
        if (searchBox.isEmpty())
        {
            if (onClose)
                onClose();
            return;
        }
        searchBox.clear();
        refresh();
    };
    searchBox.addKeyListener (this);
    addAndMakeVisible (searchBox);

    clearSearchButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
    clearSearchButton.getProperties().set ("round", true);
    clearSearchButton.setTooltip ("Clear search");
    clearSearchButton.setWantsKeyboardFocus (false);
    clearSearchButton.onClick = [this] { searchBox.clear(); refresh(); searchBox.grabKeyboardFocus(); };
    addChildComponent (clearSearchButton);

    for (auto* button : { &browseButton, &finderButton })
    {
        button->getProperties().set ("outline", true);
        button->setWantsKeyboardFocus (false);
    }
    browseButton.setTooltip ("Choose a file from anywhere on your Mac");
    browseButton.onClick = [this] { if (onBrowse) onBrowse(); };
    addAndMakeVisible (browseButton);
    finderButton.onClick = [this] { library.getRoot().revealToUser(); };
    addChildComponent (finderButton);

    closeButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
    closeButton.getProperties().set ("round", true);
    closeButton.setTooltip ("Close the library (Esc)");
    closeButton.setWantsKeyboardFocus (false);
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    list.setRowHeight (rowHeight);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    list.getViewport()->setScrollBarThickness (8);
    list.addKeyListener (this);
    addAndMakeVisible (list);

    library.addChangeListener (this);
    refresh();
}

LibraryPanel::~LibraryPanel()
{
    library.removeChangeListener (this);
    searchBox.removeKeyListener (this);
    list.removeKeyListener (this);
}

void LibraryPanel::setTarget (int slot)
{
    if (target != slot)
    {
        target = slot;
        list.repaint();
        repaint();
    }
}

void LibraryPanel::opened (const juce::File& current)
{
    library.rescan();
    refresh();
    auto row = 0;
    for (size_t i = 0; i < shown.size(); ++i)
        if (shown[i].file == current)
            row = static_cast<int> (i);
    if (! shown.empty())
    {
        list.selectRow (row);
        list.scrollToEnsureRowIsOnscreen (row);
    }
    searchBox.selectAll();
    searchBox.grabKeyboardFocus();
    startTimer (rescanMilliseconds);
}

void LibraryPanel::closed()
{
    stopTimer();
}

void LibraryPanel::timerCallback()
{
    if (! library.isReading())
        library.rescan();
}

void LibraryPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void LibraryPanel::refresh()
{
    const auto selectedRow = list.getSelectedRow();
    const auto selectedKey = juce::isPositiveAndBelow (selectedRow, static_cast<int> (shown.size())) ? shown[static_cast<size_t> (selectedRow)].key : juce::String();

    const auto entries = library.getEntries();
    totalEntries = static_cast<int> (entries.size());
    const auto words = ReferenceLibrary::parseSearch (searchBox.getText());
    shown.clear();
    for (const auto& entry : entries)
        if (ReferenceLibrary::matchesSearch (entry, words))
            shown.push_back (entry);

    list.updateContent();
    auto row = 0;
    for (size_t i = 0; i < shown.size(); ++i)
        if (shown[i].key == selectedKey)
            row = static_cast<int> (i);
    if (shown.empty())
        list.deselectAllRows();
    else
        list.selectRow (row, true, true);

    clearSearchButton.setVisible (searchBox.getText().isNotEmpty());
    showEmptyState();
    list.repaint();
    repaint();
}

void LibraryPanel::showEmptyState()
{
    const auto empty = totalEntries == 0;
    searchBox.setVisible (! empty);
    list.setVisible (! empty);
    finderButton.setVisible (empty);
    if (empty)
        clearSearchButton.setVisible (false);
}

void LibraryPanel::chooseRow (int row)
{
    if (juce::isPositiveAndBelow (row, static_cast<int> (shown.size())) && onChoose)
        onChoose (shown[static_cast<size_t> (row)].file);
}

int LibraryPanel::getNumRows()
{
    return static_cast<int> (shown.size());
}

juce::Rectangle<int> LibraryPanel::heartArea (int, int height) const
{
    return { padding - 2, 0, 28, height };
}

void LibraryPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (shown.size())))
        return;

    const auto& entry = shown[static_cast<size_t> (row)];
    auto area = juce::Rectangle<int> (0, 0, width, height).toFloat();
    if (rowIsSelected)
    {
        g.setColour (theme::panelRaised);
        g.fillRoundedRectangle (area.reduced (4.0f, 1.0f), 4.0f);
        g.setColour (theme::slotColour (target));
        g.fillRoundedRectangle (area.reduced (4.0f, 8.0f).withWidth (2.0f), 1.0f);
    }

    const auto heart = theme::heartPath (heartArea (width, height).toFloat().withSizeKeepingCentre (13.0f, 12.0f));
    if (entry.favourite)
    {
        g.setColour (theme::accent);
        g.fillPath (heart);
    }
    else
    {
        g.setColour (theme::textDim.withAlpha (rowIsSelected ? 0.8f : 0.45f));
        g.strokePath (heart, juce::PathStrokeType (1.2f));
    }

    auto right = area.withTrimmedRight (static_cast<float> (padding + 4));
    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (12.0f));
    g.drawText (formatDuration (entry.seconds), right.removeFromRight (40.0f), juce::Justification::centredRight);
    right.removeFromRight (12.0f);

    if (entry.format.isNotEmpty())
    {
        const auto badgeWidth = juce::GlyphArrangement::getStringWidth (juce::FontOptions (10.0f).withStyle ("Bold").withKerningFactor (0.08f), entry.format) + 12.0f;
        theme::paintFormatBadge (g, entry.format, right.removeFromRight (badgeWidth).withSizeKeepingCentre (badgeWidth, 16.0f));
        right.removeFromRight (12.0f);
    }

    if (const auto slot = slotOf != nullptr ? slotOf (entry.file) : -1; slot >= 0)
    {
        const auto dot = right.removeFromRight (18.0f).withSizeKeepingCentre (18.0f, 18.0f);
        g.setColour (theme::slotColour (slot));
        g.fillEllipse (dot);
        g.setColour (theme::background);
        g.setFont (theme::labelFont (11.0f).withStyle ("Bold"));
        g.drawText (juce::String (slot + 1), dot, juce::Justification::centred);
        right.removeFromRight (12.0f);
    }

    auto text = area.withLeft (static_cast<float> (heartArea (width, height).getRight() + 6)).withRight (right.getRight());
    const auto nameFont = theme::labelFont (13.5f);
    g.setFont (nameFont);
    g.setColour (rowIsSelected ? theme::text : theme::text.withAlpha (0.85f));
    const auto nameWidth = std::min (text.getWidth(), juce::GlyphArrangement::getStringWidth (nameFont, entry.name) + 2.0f);
    g.drawText (entry.name, text.removeFromLeft (nameWidth), juce::Justification::centredLeft, true);

    if (entry.folder.isNotEmpty() && text.getWidth() > 30.0f)
    {
        g.setColour (theme::textDim.withAlpha (0.8f));
        g.setFont (theme::labelFont (11.0f));
        g.drawText (entry.folder.toUpperCase(), text.withTrimmedLeft (10.0f), juce::Justification::centredLeft, true);
    }
}

void LibraryPanel::listBoxItemClicked (int row, const juce::MouseEvent& event)
{
    if (juce::isPositiveAndBelow (row, static_cast<int> (shown.size())) && heartArea (list.getWidth(), rowHeight).getHorizontalRange().contains (event.x))
        library.toggleFavourite (shown[static_cast<size_t> (row)].key);
}

void LibraryPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent& event)
{
    if (! heartArea (list.getWidth(), rowHeight).getHorizontalRange().contains (event.x))
        chooseRow (row);
}

void LibraryPanel::returnKeyPressed (int lastRowSelected)
{
    chooseRow (lastRowSelected);
}

bool LibraryPanel::keyPressed (const juce::KeyPress& key, juce::Component* originator)
{
    if (originator == &searchBox && (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey) && ! shown.empty())
    {
        const auto step = key == juce::KeyPress::downKey ? 1 : -1;
        const auto row = juce::jlimit (0, static_cast<int> (shown.size()) - 1, list.getSelectedRow() + step);
        list.selectRow (row);
        return true;
    }
    if (originator == &list && key == juce::KeyPress::escapeKey)
        return keyPressed (key);
    return false;
}

bool LibraryPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    return false;
}

bool LibraryPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension (ReferenceLibrary::audioExtensions.removeCharacters ("*"));
}

void LibraryPanel::filesDropped (const juce::StringArray& files, int, int)
{
    if (onChoose)
        onChoose (juce::File (files[0]));
}

void LibraryPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::strip);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (padding, 0);
    if (totalEntries > 0)
    {
        g.setColour (theme::outline);
        g.fillRect (1, headerHeight, getWidth() - 2, 1);
    }

    const auto into = juce::Rectangle<int>::leftTopRightBottom (searchBox.getRight() + 16, header.getY(), browseButton.getX() - 16, header.getBottom());
    const auto slotText = "REF " + juce::String (target + 1);
    const auto slotFont = theme::labelFont (12.0f).withStyle ("Bold");
    const auto slotWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (slotFont, slotText)) + 2;
    g.setColour (theme::slotColour (target));
    g.setFont (slotFont);
    g.drawText (slotText, into.withLeft (into.getRight() - slotWidth), juce::Justification::centredRight);
    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (11.0f));
    g.drawText ("LOADS INTO", into.withTrimmedRight (slotWidth + 6), juce::Justification::centredRight);

    if (totalEntries == 0)
    {
        auto message = getLocalBounds().withTrimmedTop (headerHeight).reduced (40, 0).withSizeKeepingCentre (getWidth() - 80, 110).translated (0, -24);
        g.setColour (theme::text);
        g.setFont (theme::labelFont (17.0f).withStyle ("Bold"));
        g.drawText ("Your library is empty", message.removeFromTop (28), juce::Justification::centred);
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (13.0f));
        g.drawFittedText ("Put reference tracks in Music " + juce::String::fromUTF8 ("\xe2\x80\xba") + " Lucky Reference "
                              + juce::String::fromUTF8 ("\xe2\x80\xba") + " References (folders are fine), or load a track "
                              + "into a slot and choose ADD TO LIBRARY.",
                          message.removeFromTop (44), juce::Justification::centred, 2);
    }
    else if (shown.empty())
    {
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (13.0f));
        g.drawText ("No references match \"" + searchBox.getText().trim() + "\"", list.getBounds(), juce::Justification::centred);
    }
}

void LibraryPanel::paintOverChildren (juce::Graphics& g)
{
    if (! searchBox.isVisible())
        return;

    const auto icon = searchBox.getBounds().toFloat().removeFromLeft (30.0f).withSizeKeepingCentre (11.0f, 11.0f);
    g.setColour (theme::textDim);
    g.drawEllipse (icon.withTrimmedRight (3.0f).withTrimmedBottom (3.0f), 1.3f);
    g.drawLine (icon.getRight() - 4.0f, icon.getBottom() - 4.0f, icon.getRight(), icon.getBottom(), 1.3f);
}

void LibraryPanel::resized()
{
    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (padding, (headerHeight - controlHeight) / 2);
    closeButton.setBounds (header.removeFromRight (controlHeight));
    header.removeFromRight (8);
    browseButton.setBounds (header.removeFromRight (128));
    searchBox.setBounds (header.removeFromLeft (360));
    clearSearchButton.setBounds (searchBox.getBounds().removeFromRight (controlHeight).reduced (3));

    list.setBounds (getLocalBounds().withTrimmedTop (headerHeight + 4).reduced (4, 4));
    finderButton.setBounds (getLocalBounds().withSizeKeepingCentre (150, controlHeight).translated (0, 52));
}

} // namespace lucky
