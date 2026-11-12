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
constexpr int tickMilliseconds = 500;
constexpr int rescanTicks = 10;
constexpr int hintHeight = 26;
constexpr float matchBarWidth = 56.0f;
/** Room for most format badges, so the match bars line up; longer badges such as "MP3 · 320 kbps" take more. */
constexpr float formatColumnWidth = 96.0f;
constexpr float slotButtonSize = 20.0f;
constexpr float slotButtonGap = 6.0f;
constexpr juce::uint32 noticeMilliseconds = 3500;

/** How many of the closest tracks have to stay the same, in order, before UPDATE is offered. */
constexpr size_t watchedTopMatches = 3;

/** The keys of the closest analysed tracks, in order. */
juce::StringArray topKeys (const std::vector<LibraryEntry>& rankedEntries)
{
    juce::StringArray keys;
    for (const auto& entry : rankedEntries)
        if (entry.profile && static_cast<size_t> (keys.size()) < watchedTopMatches)
            keys.add (entry.key);
    return keys;
}

juce::String formatDuration (double seconds)
{
    if (seconds <= 0.0)
        return {};
    const auto total = juce::roundToInt (seconds);
    return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}

bool isAudioFile (const juce::File& file)
{
    return file.existsAsFile() && file.hasFileExtension (ReferenceLibrary::audioExtensions.removeCharacters ("*"));
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
    searchBox.onReturnKey = [this] { chooseRowIntoEmptySlot (list.getSelectedRow()); };
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

    for (auto* button : { &addButton, &finderButton })
    {
        button->getProperties().set ("outline", true);
        button->setWantsKeyboardFocus (false);
    }
    addButton.setTooltip ("Copy audio files into your library");
    addButton.onClick = [this] { browseForFiles(); };
    addAndMakeVisible (addButton);
    finderButton.onClick = [this] { library.getRoot().revealToUser(); };
    addChildComponent (finderButton);

    for (auto* button : { &nameOrderButton, &closestOrderButton })
    {
        button->setWantsKeyboardFocus (false);
        button->setRadioGroupId (1);
        button->setColour (juce::Slider::rotarySliderFillColourId, theme::mixColour);
        addAndMakeVisible (*button);
    }
    nameOrderButton.setTooltip ("Sort by name");
    closestOrderButton.setTooltip ("Closest to your mix first, by tonal balance and punch");
    nameOrderButton.onClick = [this] { setClosest (false); };
    closestOrderButton.onClick = [this] { setClosest (true); };
    nameOrderButton.setToggleState (true, juce::dontSendNotification);

    updateButton.getProperties().set ("outline", true);
    updateButton.setWantsKeyboardFocus (false);
    updateButton.setTooltip ("Re-rank the list against your mix as it is now");
    updateButton.onClick = [this]
    {
        rank();
        refresh();
        list.scrollToEnsureRowIsOnscreen (0);
        if (! shown.empty())
            list.selectRow (0);
    };
    addChildComponent (updateButton);

    list.setRowHeight (rowHeight);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    list.getViewport()->setScrollBarThickness (8);
    list.setWantsKeyboardFocus (false);
    list.addMouseListener (this, true);
    addAndMakeVisible (list);

    replaceCard.replaceButton.onClick = [this] { closeReplaceCard (true); };
    replaceCard.cancelButton.onClick = [this] { closeReplaceCard (false); };
    addChildComponent (replaceCard);

    setWantsKeyboardFocus (true);
    library.addChangeListener (this);
    refresh();
}

LibraryPanel::~LibraryPanel()
{
    library.removeChangeListener (this);
    searchBox.removeKeyListener (this);
    list.removeMouseListener (this);
}

void LibraryPanel::opened()
{
    library.rescan();
    rank();
    refresh();
    if (! shown.empty() && list.getSelectedRow() < 0)
        list.selectRow (0);
    grabKeyboardFocus();
    ticks = 0;
    startTimer (tickMilliseconds);
}

void LibraryPanel::closed()
{
    if (replaceCard.isVisible())
        closeReplaceCard (false);
    stopTimer();
}

void LibraryPanel::timerCallback()
{
    if (++ticks % rescanTicks == 0 && ! library.isReading())
        library.rescan();

    if (sortClosest && ! rankedAgainst)
        if (rank(); rankedAgainst)
        {
            refresh();
            list.scrollToEnsureRowIsOnscreen (0);
            if (! shown.empty())
                list.selectRow (0);
        }

    checkTopMatches();

    if (notice.isNotEmpty() && juce::Time::getMillisecondCounter() > noticeUntil && importing.isEmpty())
        notice.clear();

    repaint();
}

void LibraryPanel::setClosest (bool closest)
{
    sortClosest = closest;
    nameOrderButton.setToggleState (! closest, juce::dontSendNotification);
    closestOrderButton.setToggleState (closest, juce::dontSendNotification);
    rank();
    resized();
    refresh();
    list.scrollToEnsureRowIsOnscreen (0);
    if (! shown.empty())
        list.selectRow (0);
}

void LibraryPanel::rank()
{
    rankedAgainst = sortClosest && mixProfile != nullptr ? mixProfile() : std::nullopt;
    topMatchesChanged = false;
}

void LibraryPanel::checkTopMatches()
{
    if (sortClosest && rankedAgainst && ! topMatchesChanged && mixProfile != nullptr)
        if (const auto now = mixProfile())
        {
            auto reranked = shown;
            ReferenceLibrary::sortClosestFirst (reranked, *now);
            topMatchesChanged = topKeys (reranked) != topKeys (shown);
        }
    updateButton.setVisible (topMatchesChanged && list.isVisible());
}

bool LibraryPanel::waitingForMix() const
{
    return sortClosest && ! rankedAgainst && totalEntries > 0;
}

juce::String LibraryPanel::hintText() const
{
    if (! sortClosest || ! rankedAgainst || totalEntries == 0)
        return {};

    juce::String text = topMatchesChanged ? "Your top matches have changed" : "Closest to your mix by tonal balance and punch";
    if (analysedEntries < totalEntries)
        text += juce::String::fromUTF8 ("  \xc2\xb7  ") + "analysing " + juce::String (analysedEntries) + " of " + juce::String (totalEntries);
    return text;
}

void LibraryPanel::showNotice (const juce::String& text)
{
    notice = text;
    noticeUntil = juce::Time::getMillisecondCounter() + noticeMilliseconds;
    repaint();
}

void LibraryPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    auto finished = 0, failed = 0;
    for (int i = importing.size(); --i >= 0;)
        if (const auto copy = library.takeImportResult (importing[i]))
        {
            ++finished;
            if (! copy->existsAsFile())
                ++failed;
            importing.remove (i);
        }

    if (finished > 0)
    {
        if (importing.isEmpty())
            showNotice (failed > 0 ? juce::String (failed) + (failed == 1 ? " file" : " files") + " couldn't be copied"
                                   : juce::String ("Added to your library"));
        library.rescan();
        return;
    }
    refresh();
}

void LibraryPanel::addFiles (const juce::Array<juce::File>& files)
{
    for (const auto& file : files)
        if (isAudioFile (file) && ! library.contains (file) && ! importing.contains (file))
        {
            importing.add (file);
            library.importFileAsync (file);
        }

    if (! importing.isEmpty())
    {
        notice = "Adding " + juce::String (importing.size()) + (importing.size() == 1 ? " file..." : " files...");
        repaint();
    }
}

void LibraryPanel::browseForFiles()
{
    chooser = std::make_unique<juce::FileChooser> ("Add to your library", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   ReferenceLibrary::audioExtensions);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this] (const juce::FileChooser& finished) { addFiles (finished.getResults()); });
}

void LibraryPanel::refresh()
{
    const auto selectedRow = list.getSelectedRow();
    const auto selectedKey = juce::isPositiveAndBelow (selectedRow, static_cast<int> (shown.size())) ? shown[static_cast<size_t> (selectedRow)].key : juce::String();

    const auto entries = library.getEntries();
    totalEntries = static_cast<int> (entries.size());
    analysedEntries = static_cast<int> (std::count_if (entries.begin(), entries.end(), [] (const LibraryEntry& entry) { return entry.analysed; }));
    const auto words = ReferenceLibrary::parseSearch (searchBox.getText());
    shown.clear();
    for (const auto& entry : entries)
        if (ReferenceLibrary::matchesSearch (entry, words))
            shown.push_back (entry);
    if (sortClosest && rankedAgainst)
        ReferenceLibrary::sortClosestFirst (shown, *rankedAgainst);

    list.updateContent();
    auto row = 0;
    for (size_t i = 0; i < shown.size(); ++i)
        if (shown[i].key == selectedKey)
            row = static_cast<int> (i);
    if (shown.empty())
        list.deselectAllRows();
    else
        list.selectRow (row, true, true);

    updateVisibility();
    list.repaint();
    repaint();
}

void LibraryPanel::updateVisibility()
{
    const auto empty = totalEntries == 0;
    searchBox.setVisible (! empty);
    clearSearchButton.setVisible (! empty && searchBox.getText().isNotEmpty());
    nameOrderButton.setVisible (! empty);
    closestOrderButton.setVisible (! empty);
    list.setVisible (! empty && ! waitingForMix());
    finderButton.setVisible (empty);
    updateButton.setVisible (topMatchesChanged && list.isVisible());
    resized();
}

void LibraryPanel::chooseRow (int row, int slot)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (shown.size())) || ! juce::isPositiveAndBelow (slot, numSlots))
        return;

    chooseFile (slot, shown[static_cast<size_t> (row)].file);
}

void LibraryPanel::chooseFile (int slot, const juce::File& file)
{
    const auto current = slotFile != nullptr ? slotFile (slot) : juce::File();
    if (current != juce::File() && current != file)
        askToReplace (slot, file);
    else
        load (slot, file);
}

void LibraryPanel::load (int slot, const juce::File& file)
{
    if (onChoose)
        onChoose (slot, file);
    showNotice ("Loaded into reference " + juce::String (slot + 1));
    list.repaint();
}

void LibraryPanel::askToReplace (int slot, const juce::File& file)
{
    const auto current = slotFile (slot);
    pendingSlot = slot;
    pendingFile = file;
    const auto quote = [] (const juce::String& text) { return juce::String::fromUTF8 ("\xe2\x80\x9c") + text + juce::String::fromUTF8 ("\xe2\x80\x9d"); };
    replaceCard.show ("Replace " + quote (current.getFileNameWithoutExtension()) + " in reference " + juce::String (slot + 1) + "?",
                      library.contains (current) ? juce::String ("It stays in your library.")
                                                 : juce::String ("It isn't in your library, so you'd have to find the file again to get it back."),
                      theme::slotColour (slot));
    replaceCard.setBounds (list.getBounds());
    grabKeyboardFocus();
}

void LibraryPanel::closeReplaceCard (bool replace)
{
    replaceCard.setVisible (false);
    if (replace && pendingSlot >= 0)
        load (pendingSlot, pendingFile);
    pendingSlot = -1;
    grabKeyboardFocus();
}

LibraryPanel::ReplaceCard::ReplaceCard()
{
    replaceButton.getProperties().set ("outline", true);
    replaceButton.getProperties().set ("primary", true);
    cancelButton.getProperties().set ("outline", true);
    for (auto* button : { &replaceButton, &cancelButton })
    {
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    setInterceptsMouseClicks (true, true);
}

void LibraryPanel::ReplaceCard::show (const juce::String& newTitle, const juce::String& newDetail, juce::Colour slotColour)
{
    title = newTitle;
    detail = newDetail;
    replaceButton.setColour (juce::Slider::rotarySliderFillColourId, slotColour);
    setVisible (true);
    toFront (false);
    repaint();
}

void LibraryPanel::ReplaceCard::paint (juce::Graphics& g)
{
    g.setColour (theme::strip.withAlpha (0.85f));
    g.fillRect (getLocalBounds());
    const auto card = getLocalBounds().withSizeKeepingCentre (460, 150).toFloat();
    g.setColour (theme::panelRaised);
    g.fillRoundedRectangle (card, 6.0f);
    g.setColour (theme::outline.brighter (0.2f));
    g.drawRoundedRectangle (card.reduced (0.5f), 6.0f, 1.0f);

    auto text = card.reduced (24.0f, 20.0f);
    g.setColour (theme::text);
    g.setFont (theme::labelFont (15.0f).withStyle ("Bold"));
    g.drawFittedText (title, text.removeFromTop (22.0f).toNearestInt(), juce::Justification::centredLeft, 1);
    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (12.5f));
    g.drawFittedText (detail, text.removeFromTop (40.0f).toNearestInt(), juce::Justification::centredLeft, 2);
}

void LibraryPanel::ReplaceCard::resized()
{
    auto buttons = getLocalBounds().withSizeKeepingCentre (460, 150).reduced (24, 20).removeFromBottom (30);
    replaceButton.setBounds (buttons.removeFromRight (110));
    buttons.removeFromRight (10);
    cancelButton.setBounds (buttons.removeFromRight (100));
}

void LibraryPanel::chooseRowIntoEmptySlot (int row)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (shown.size())))
        return;

    for (int slot = 0; slot < numSlots; ++slot)
        if (slotFile != nullptr && slotFile (slot) == juce::File())
        {
            chooseRow (row, slot);
            return;
        }
    showNotice ("All three references are loaded. Click 1, 2 or 3 to replace one");
}

int LibraryPanel::getNumRows()
{
    return static_cast<int> (shown.size());
}

juce::Rectangle<float> LibraryPanel::slotButtonArea (int slot, int width, int height) const
{
    const auto right = static_cast<float> (width - padding) - static_cast<float> (numSlots - 1 - slot) * (slotButtonSize + slotButtonGap);
    return { right - slotButtonSize, (static_cast<float> (height) - slotButtonSize) * 0.5f, slotButtonSize, slotButtonSize };
}

int LibraryPanel::slotButtonAt (juce::Point<int> position, int width, int height) const
{
    for (int slot = 0; slot < numSlots; ++slot)
        if (slotButtonArea (slot, width, height).expanded (slotButtonGap * 0.5f, 4.0f).contains (position.toFloat()))
            return slot;
    return -1;
}

void LibraryPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (shown.size())))
        return;

    const auto& entry = shown[static_cast<size_t> (row)];
    auto area = juce::Rectangle<int> (0, 0, width, height).toFloat();
    const auto lit = rowIsSelected || row == hoverRow;
    if (rowIsSelected)
    {
        g.setColour (theme::panelRaised);
        g.fillRoundedRectangle (area.reduced (4.0f, 1.0f), 4.0f);
    }

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto button = slotButtonArea (slot, width, height);
        const auto colour = theme::slotColour (slot);
        const auto loaded = slotFile != nullptr && slotFile (slot) == entry.file;
        const auto hot = row == hoverRow && slot == hoverSlot;
        if (loaded || hot)
        {
            g.setColour (loaded ? colour : colour.withAlpha (0.2f));
            g.fillRoundedRectangle (button, 4.0f);
        }
        g.setColour (loaded || hot ? colour : (lit ? colour.withAlpha (0.55f) : theme::outline.brighter (0.2f)));
        g.drawRoundedRectangle (button.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (loaded ? theme::background : (hot ? theme::text : (lit ? colour : theme::textDim.withAlpha (0.6f))));
        g.setFont (theme::labelFont (11.0f).withStyle ("Bold"));
        g.drawText (juce::String (slot + 1), button, juce::Justification::centred);
    }

    auto right = area.withRight (slotButtonArea (0, width, height).getX() - 16.0f);
    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (12.0f));
    g.drawText (formatDuration (entry.seconds), right.removeFromRight (40.0f), juce::Justification::centredRight);
    right.removeFromRight (12.0f);

    const auto badgeWidth = juce::GlyphArrangement::getStringWidth (juce::FontOptions (10.0f).withStyle ("Bold").withKerningFactor (0.08f), entry.format) + 12.0f;
    auto badgeColumn = right.removeFromRight (std::max (formatColumnWidth, badgeWidth));
    if (entry.format.isNotEmpty())
        theme::paintFormatBadge (g, entry.format, badgeColumn.removeFromRight (badgeWidth).withSizeKeepingCentre (badgeWidth, 16.0f));
    right.removeFromRight (12.0f);

    if (sortClosest && rankedAgainst)
    {
        auto barArea = right.removeFromRight (matchBarWidth).withSizeKeepingCentre (matchBarWidth, 4.0f);
        right.removeFromRight (14.0f);
        if (entry.profile)
        {
            g.setColour (theme::outline);
            g.fillRoundedRectangle (barArea, 2.0f);
            g.setColour (theme::mixColour.withAlpha (0.85f));
            g.fillRoundedRectangle (barArea.withWidth (std::max (4.0f, matchBarWidth * TonalProfile::match (*entry.profile, *rankedAgainst))), 2.0f);
        }
        else
        {
            g.setColour (theme::textDim.withAlpha (0.6f));
            g.setFont (theme::labelFont (10.0f));
            g.drawText (entry.analysed ? "SILENT" : "...", barArea.withSizeKeepingCentre (matchBarWidth, 16.0f), juce::Justification::centred);
        }
    }

    auto text = area.withLeft (static_cast<float> (padding + 4)).withRight (right.getRight());
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
    if (const auto slot = slotButtonAt (event.getPosition(), list.getVisibleRowWidth(), rowHeight); slot >= 0)
        chooseRow (row, slot);
}

void LibraryPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent& event)
{
    if (slotButtonAt (event.getPosition(), list.getVisibleRowWidth(), rowHeight) < 0)
        chooseRowIntoEmptySlot (row);
}

void LibraryPanel::returnKeyPressed (int lastRowSelected)
{
    chooseRowIntoEmptySlot (lastRowSelected);
}

void LibraryPanel::mouseMove (const juce::MouseEvent& event)
{
    auto row = -1, slot = -1;
    if (event.eventComponent != this && list.isVisible())
    {
        const auto inList = event.getEventRelativeTo (&list).getPosition();
        row = list.getRowContainingPosition (inList.x, inList.y);
        if (row >= 0)
            slot = slotButtonAt (inList - list.getRowPosition (row, true).getPosition(), list.getVisibleRowWidth(), rowHeight);
    }

    if (row != hoverRow || slot != hoverSlot)
    {
        hoverRow = row;
        hoverSlot = slot;
        list.repaint();
    }
}

void LibraryPanel::mouseExit (const juce::MouseEvent& event)
{
    if (! list.getScreenBounds().contains (event.getScreenPosition()) && (hoverRow >= 0 || hoverSlot >= 0))
    {
        hoverRow = hoverSlot = -1;
        list.repaint();
    }
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
    return false;
}

bool LibraryPanel::keyPressed (const juce::KeyPress& key)
{
    if (replaceCard.isVisible())
    {
        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::escapeKey)
        {
            closeReplaceCard (key == juce::KeyPress::returnKey);
            return true;
        }
        return false;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    if (! list.isVisible() || shown.empty())
        return false;

    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        const auto row = juce::jlimit (0, static_cast<int> (shown.size()) - 1, list.getSelectedRow() + (key == juce::KeyPress::downKey ? 1 : -1));
        list.selectRow (row);
        return true;
    }
    if (key == juce::KeyPress::returnKey)
    {
        chooseRowIntoEmptySlot (list.getSelectedRow());
        return true;
    }
    if (const auto character = key.getTextCharacter(); character >= '1' && character < '1' + numSlots && ! key.getModifiers().isAnyModifierKeyDown())
    {
        chooseRow (list.getSelectedRow(), static_cast<int> (character - '1'));
        return true;
    }
    return false;
}

void LibraryPanel::mouseDown (const juce::MouseEvent&)
{
    grabKeyboardFocus();
}

bool LibraryPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (isAudioFile (juce::File (path)))
            return true;
    return false;
}

void LibraryPanel::filesDropped (const juce::StringArray& files, int, int)
{
    juce::Array<juce::File> dropped;
    for (const auto& path : files)
        dropped.add (juce::File (path));
    addFiles (dropped);
}

void LibraryPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::strip);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    if (totalEntries > 0)
    {
        g.setColour (theme::outline);
        g.fillRect (1, headerHeight, getWidth() - 2, 1);
    }

    if (notice.isNotEmpty())
    {
        const auto area = juce::Rectangle<int>::leftTopRightBottom (closestOrderButton.isVisible() ? closestOrderButton.getRight() + 16 : padding, 0,
                                                                    addButton.getX() - 16, headerHeight);
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (11.5f));
        g.drawFittedText (notice, area, juce::Justification::centredRight, 2);
    }

    if (const auto hint = hintText(); hint.isNotEmpty())
    {
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (11.5f));
        g.drawText (hint, juce::Rectangle<int> (padding + 4, headerHeight + 2, getWidth() - 2 * padding - 8, hintHeight).withRight (updateButton.isVisible() ? updateButton.getX() - 12 : getWidth() - padding - 4),
                    juce::Justification::centredLeft, true);
    }

    if (totalEntries == 0)
    {
        auto message = getLocalBounds().withTrimmedTop (headerHeight).reduced (40, 0).withSizeKeepingCentre (getWidth() - 80, 110).translated (0, -24);
        g.setColour (theme::text);
        g.setFont (theme::labelFont (17.0f).withStyle ("Bold"));
        g.drawText ("Your library is empty", message.removeFromTop (28), juce::Justification::centred);
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (13.0f));
        const auto arrow = juce::String::fromUTF8 (" \xe2\x80\xba ");
        g.drawFittedText ("Put reference tracks in Music" + arrow + "Lucky Reference" + arrow + "References, add a folder from the "
                              + juce::String::fromUTF8 ("\xe2\x8b\xaf") + " menu, or add files here.",
                          message.removeFromTop (44), juce::Justification::centred, 2);
    }
    else if (waitingForMix())
    {
        const auto heard = juce::jlimit (0.0, mixSecondsNeeded, mixSeconds != nullptr ? mixSeconds() : 0.0);
        auto message = getLocalBounds().withTrimmedTop (headerHeight).withSizeKeepingCentre (440, 110);
        g.setColour (theme::text);
        g.setFont (theme::labelFont (17.0f).withStyle ("Bold"));
        g.drawText ("Play your mix for " + juce::String (juce::roundToInt (mixSecondsNeeded)) + " seconds", message.removeFromTop (28), juce::Justification::centred);
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (13.0f));
        g.drawFittedText ("Then your library is ranked by how close each track's tonal balance and punch are to your mix.",
                          message.removeFromTop (44), juce::Justification::centred, 2);
        const auto bar = message.removeFromTop (20).toFloat().withSizeKeepingCentre (200.0f, 4.0f);
        g.setColour (theme::outline);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (theme::mixColour);
        g.fillRoundedRectangle (bar.withWidth (std::max (4.0f, bar.getWidth() * static_cast<float> (heard / mixSecondsNeeded))), 2.0f);
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
    const auto empty = totalEntries == 0;
    if (! empty)
        addButton.setBounds (header.removeFromRight (104));
    searchBox.setBounds (header.removeFromLeft (300));
    clearSearchButton.setBounds (searchBox.getBounds().removeFromRight (controlHeight).reduced (3));
    header.removeFromLeft (10);
    nameOrderButton.setBounds (header.removeFromLeft (52));
    closestOrderButton.setBounds (header.removeFromLeft (82));

    list.setBounds (getLocalBounds().withTrimmedTop (headerHeight + 4 + (sortClosest ? hintHeight : 0)).reduced (4, 4));
    updateButton.setBounds (juce::Rectangle<int> (getWidth() - padding - 4 - 96, headerHeight + 4, 96, hintHeight - 4));
    if (empty)
    {
        auto buttons = getLocalBounds().withSizeKeepingCentre (318, controlHeight).translated (0, 52);
        addButton.setBounds (buttons.removeFromLeft (150));
        finderButton.setBounds (buttons.removeFromRight (150));
    }
}

} // namespace lucky
