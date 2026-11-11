#include "SlotRow.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr float badgeWidth = 44.0f;
constexpr float edgeGrabPixels = 6.0f;
constexpr float dragThresholdPixels = 4.0f;
} // namespace

SlotRow::SlotRow (LuckyReferenceProcessor& owner, int index) : processor (owner), slot (index)
{
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    processor.getLibrary().addChangeListener (this);
}

SlotRow::~SlotRow()
{
    processor.getLibrary().removeChangeListener (this);
}

void SlotRow::setState (bool isSelected, bool isAudible)
{
    if (selected != isSelected || audible != isAudible)
    {
        selected = isSelected;
        audible = isAudible;
        repaint();
    }
}

juce::Rectangle<float> SlotRow::waveformArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (badgeWidth).reduced (10.0f, 8.0f).withTrimmedTop (20.0f);
}

double SlotRow::secondsAt (float x) const
{
    const auto track = processor.getTrack (slot);
    const auto area = waveformArea();
    if (track == nullptr || area.getWidth() <= 0.0f)
        return 0.0;
    return juce::jlimit (0.0, track->getLengthSeconds(), static_cast<double> ((x - area.getX()) / area.getWidth()) * track->getLengthSeconds());
}

float SlotRow::xAt (double seconds) const
{
    const auto track = processor.getTrack (slot);
    const auto area = waveformArea();
    if (track == nullptr || track->getLengthSeconds() <= 0.0)
        return area.getX();
    return area.getX() + static_cast<float> (seconds / track->getLengthSeconds()) * area.getWidth();
}

LoopRegion SlotRow::shownLoop() const
{
    return draft.value_or (processor.getLoop (slot));
}

SlotRow::Drag SlotRow::edgeAt (float x) const
{
    const auto loop = shownLoop();
    if (! loop.isSet())
        return Drag::none;
    if (std::abs (x - xAt (loop.startSeconds)) <= edgeGrabPixels)
        return Drag::startEdge;
    if (std::abs (x - xAt (loop.endSeconds)) <= edgeGrabPixels)
        return Drag::endEdge;
    return Drag::none;
}

void SlotRow::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto colour = theme::slotColour (slot);
    const auto track = processor.getTrack (slot);

    g.setColour (selected ? theme::panelRaised : theme::strip);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (selected || dropHover ? colour.withAlpha (dropHover ? 0.9f : 0.55f) : theme::outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    auto badge = bounds.withWidth (badgeWidth);
    if (audible)
    {
        g.setColour (colour);
        g.fillRoundedRectangle (badge.withSizeKeepingCentre (26.0f, 26.0f), 13.0f);
    }
    g.setColour (audible ? theme::background : (selected ? colour : colour.withAlpha (0.7f)));
    g.setFont (theme::labelFont (15.0f).withStyle ("Bold"));
    g.drawText (juce::String (slot + 1), badge, juce::Justification::centred);

    const auto area = waveformArea();
    const auto label = area.withY (bounds.getY() + 6.0f).withHeight (17.0f);
    g.setFont (theme::labelFont (12.5f));

    const auto file = processor.getFile (slot);
    if (track == nullptr)
    {
        juce::String message = "Drop a reference here, or click to choose a file";
        if (processor.isLoading (slot))
            message = "Loading " + file.getFileName() + "...";
        else if (processor.getError (slot).isNotEmpty())
            message = processor.getError (slot) + ": " + file.getFileName() + ". Click to find it";

        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (14.0f));
        g.drawText (message, area.withTrimmedTop (-20.0f), juce::Justification::centred);
        if (! processor.isLoading (slot))
        {
            g.setColour (theme::outline);
            const float dashes[] { 4.0f, 4.0f };
            juce::Path outline;
            outline.addRoundedRectangle (area.withTrimmedTop (-14.0f), 4.0f);
            juce::Path dashed;
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
            g.fillPath (dashed);
        }
        return;
    }

    const auto loop = shownLoop();
    const auto range = loop.resolve (track->getLength(), track->getSampleRate());
    const auto loopStartX = loop.isSet() ? xAt (static_cast<double> (range.getStart()) / track->getSampleRate()) : area.getX();
    const auto loopEndX = loop.isSet() ? xAt (static_cast<double> (range.getEnd()) / track->getSampleRate()) : area.getRight();

    if (loop.isSet())
    {
        g.setColour (colour.withAlpha (0.12f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (loopStartX, area.getY(), loopEndX, area.getBottom()));
    }

    const auto& overview = track->getOverview();
    const auto columns = std::max (1, static_cast<int> (area.getWidth() / 2.0f));
    const auto centreY = area.getCentreY();
    const auto loudestPeak = std::max (1.0e-4f, *std::max_element (overview.begin(), overview.end()));
    for (int column = 0; column < columns; ++column)
    {
        const auto x = area.getX() + static_cast<float> (column) * 2.0f;
        const auto first = static_cast<size_t> (column * static_cast<int> (overview.size()) / columns);
        const auto last = std::max (first + 1, static_cast<size_t> ((column + 1) * static_cast<int> (overview.size()) / columns));
        float peak = 0.0f;
        for (auto i = first; i < last && i < overview.size(); ++i)
            peak = std::max (peak, overview[i]);

        const auto height = std::max (1.0f, std::sqrt (peak / loudestPeak) * area.getHeight() * 0.5f);
        const auto inside = x + 1.0f >= loopStartX && x <= loopEndX;
        g.setColour (inside ? colour.withAlpha (selected ? 0.95f : 0.7f) : theme::textDim.withAlpha (0.35f));
        g.fillRect (x, centreY - height, 1.2f, height * 2.0f);
    }

    if (loop.isSet())
    {
        g.setColour (colour);
        for (const auto x : { loopStartX, loopEndX })
            g.fillRect (x - 1.0f, area.getY() - 2.0f, 2.0f, area.getHeight() + 4.0f);
    }

    if (selected)
    {
        const auto playheadX = xAt (static_cast<double> (processor.getEngine().getPosition (slot)) / track->getSampleRate());
        g.setColour (theme::text.withAlpha (audible ? 0.95f : 0.4f));
        g.fillRect (playheadX - 0.5f, area.getY() - 2.0f, 1.0f, area.getHeight() + 4.0f);
    }

    removeButtonArea = label.withLeft (label.getRight() - 18.0f).withSizeKeepingCentre (18.0f, 18.0f);
    auto labelArea = label.withTrimmedRight (28.0f);
    if (hovered)
    {
        if (removeHover)
        {
            g.setColour (theme::text.withAlpha (0.1f));
            g.fillEllipse (removeButtonArea);
        }
        g.setColour (removeHover ? theme::text : theme::textDim);
        const auto cross = removeButtonArea.withSizeKeepingCentre (7.0f, 7.0f);
        g.drawLine ({ cross.getTopLeft(), cross.getBottomRight() }, 1.4f);
        g.drawLine ({ cross.getBottomLeft(), cross.getTopRight() }, 1.4f);
    }

    const auto lengthText = loop.isSet() ? juce::String (static_cast<double> (range.getLength()) / track->getSampleRate(), 1) + " s loop"
                                         : juce::String ("Drag to choose a loop");
    g.setColour (theme::textDim);
    g.drawText (lengthText, labelArea, juce::Justification::centredRight, true);

    const auto nameFont = g.getCurrentFont();
    auto nameArea = labelArea.withTrimmedRight (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), lengthText) + 24.0f);

    addButtonArea = {};
    if (canAddToLibrary())
    {
        const auto text = adding ? juce::String ("ADDING...") : juce::String ("+ ADD TO LIBRARY");
        const auto font = theme::labelFont (10.0f).withStyle ("Bold");
        const auto width = juce::GlyphArrangement::getStringWidth (font, text) + 16.0f;
        addButtonArea = nameArea.removeFromRight (width).withSizeKeepingCentre (width, 18.0f);
        nameArea.removeFromRight (12.0f);
        const auto hot = addHover && ! adding;
        g.setColour (hot ? colour.withAlpha (0.16f) : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (addButtonArea, 9.0f);
        g.setColour (hot ? colour.withAlpha (0.9f) : theme::outline.brighter (0.3f));
        g.drawRoundedRectangle (addButtonArea.reduced (0.5f), 9.0f, 1.0f);
        g.setColour (hot ? theme::text : theme::textDim);
        g.setFont (font);
        g.drawText (text, addButtonArea, juce::Justification::centred);
        g.setFont (nameFont);
    }

    const auto& format = track->getFormat();
    if (format.isNotEmpty())
    {
        const auto formatWidth = juce::GlyphArrangement::getStringWidth (theme::labelFont (10.0f).withStyle ("Bold"), format) + 12.0f;
        const auto nameWidth = juce::GlyphArrangement::getStringWidth (nameFont, track->getName());
        const auto tagX = std::min (nameArea.getX() + nameWidth + 10.0f, nameArea.getRight() - formatWidth);
        const auto tag = juce::Rectangle<float> (tagX, label.getCentreY() - 8.0f, formatWidth, 16.0f);
        theme::paintFormatBadge (g, format, tag);
        g.setFont (nameFont);
        nameArea.setRight (tag.getX() - 10.0f);
    }

    g.setColour (selected ? theme::text : theme::textDim);
    g.drawText (track->getName(), nameArea, juce::Justification::centredLeft, true);
}

void SlotRow::mouseMove (const juce::MouseEvent& event)
{
    const auto loaded = processor.getTrack (slot) != nullptr;
    const auto overAdd = addButtonArea.contains (event.position);
    const auto overRemove = loaded && removeButtonArea.expanded (3.0f).contains (event.position);
    if (overAdd != addHover || overRemove != removeHover || ! hovered)
    {
        addHover = overAdd;
        removeHover = overRemove;
        hovered = true;
        repaint();
    }
    const auto overEdge = ! overAdd && ! overRemove && loaded && edgeAt (event.position.x) != Drag::none;
    setMouseCursor (overEdge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::PointingHandCursor);
}

void SlotRow::mouseEnter (const juce::MouseEvent& event)
{
    mouseMove (event);
}

void SlotRow::mouseExit (const juce::MouseEvent&)
{
    if (addHover || removeHover || hovered)
    {
        addHover = removeHover = hovered = false;
        repaint();
    }
}

void SlotRow::mouseDown (const juce::MouseEvent& event)
{
    if (onSelect)
        onSelect();

    if (event.mods.isPopupMenu())
    {
        showMenu();
        return;
    }

    if (processor.getTrack (slot) == nullptr)
    {
        if (! processor.isLoading (slot))
            browse();
        return;
    }

    if (removeButtonArea.expanded (3.0f).contains (event.position))
    {
        drag = Drag::blocked;
        processor.clearSlot (slot);
        return;
    }

    if (addButtonArea.contains (event.position))
    {
        drag = Drag::blocked;
        addToLibrary();
        return;
    }

    drag = edgeAt (event.position.x);
    dragAnchorSeconds = secondsAt (event.position.x);
}

void SlotRow::mouseDrag (const juce::MouseEvent& event)
{
    if (processor.getTrack (slot) == nullptr || event.mods.isPopupMenu() || drag == Drag::blocked)
        return;

    const auto seconds = secondsAt (event.position.x);
    auto loop = shownLoop();
    switch (drag)
    {
        case Drag::none:
            if (static_cast<float> (event.getDistanceFromDragStart()) < dragThresholdPixels)
                return;
            drag = Drag::newLoop;
            [[fallthrough]];
        case Drag::newLoop:
            loop = { std::min (dragAnchorSeconds, seconds), std::max (dragAnchorSeconds, seconds) };
            break;
        case Drag::startEdge:
            loop.startSeconds = std::min (seconds, loop.endSeconds - LoopRegion::minimumSeconds);
            break;
        case Drag::endEdge:
            loop.endSeconds = std::max (seconds, loop.startSeconds + LoopRegion::minimumSeconds);
            break;
        case Drag::blocked:
            return;
    }

    loop.startSeconds = std::max (0.0, loop.startSeconds);
    draft = loop;
    repaint();
}

void SlotRow::mouseUp (const juce::MouseEvent& event)
{
    if (processor.getTrack (slot) == nullptr || event.mods.isPopupMenu())
        return;
    if (drag == Drag::blocked)
    {
        drag = Drag::none;
        return;
    }

    if (draft)
    {
        processor.setLoop (slot, *draft);
        draft.reset();
    }
    else if (drag == Drag::none && event.position.x >= waveformArea().getX())
    {
        processor.seek (slot, secondsAt (event.position.x));
    }

    drag = Drag::none;
    repaint();
}

void SlotRow::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (processor.getTrack (slot) != nullptr && ! event.mods.isPopupMenu())
        processor.setLoop (slot, {});
}

void SlotRow::showMenu()
{
    juce::PopupMenu menu;
    const auto hasFile = processor.getFile (slot) != juce::File();
    menu.addItem (hasFile ? "Replace with a file..." : "Choose a file...", [this] { browse(); });
    menu.addItem ("Add to library", canAddToLibrary() && ! adding, false, [this] { addToLibrary(); });
    menu.addItem ("Clear loop", processor.getLoop (slot).isSet(), false, [this] { processor.setLoop (slot, {}); });
    menu.addSeparator();
    menu.addItem ("Remove reference", hasFile, false, [this] { processor.clearSlot (slot); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

bool SlotRow::canAddToLibrary() const
{
    const auto file = processor.getFile (slot);
    return settings->hasOpenedLibrary() && processor.getTrack (slot) != nullptr && file.existsAsFile() && ! processor.getLibrary().contains (file);
}

void SlotRow::addToLibrary()
{
    if (adding || ! canAddToLibrary())
        return;

    adding = true;
    importing = processor.getFile (slot);
    repaint();
    processor.getLibrary().importFileAsync (importing);
}

void SlotRow::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (! adding)
        return;

    auto& library = processor.getLibrary();
    const auto copy = library.takeImportResult (importing);
    if (! copy)
        return;

    adding = false;
    if (copy->existsAsFile())
    {
        if (processor.getFile (slot) == importing)
            processor.relinkFile (slot, *copy);
        library.rescan();
    }
    repaint();
}

juce::File SlotRow::browseStartFolder() const
{
    auto candidates = juce::Array<juce::File> { processor.getFile (slot) };
    for (int other = 0; other < LuckyReferenceProcessor::numSlots; ++other)
        if (other != slot)
            candidates.add (processor.getFile (other));

    for (const auto& file : candidates)
        if (file != juce::File() && file.getParentDirectory().isDirectory())
            return file.getParentDirectory();

    if (const auto last = settings->getLastReferenceFolder(); last.isDirectory())
        return last;
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory);
}

void SlotRow::loadAndRemember (const juce::File& file)
{
    settings->setLastReferenceFolder (file.getParentDirectory());
    processor.loadFile (slot, file);
}

void SlotRow::browse()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose reference " + juce::String (slot + 1), browseStartFolder(), audioExtensions);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& finished)
                          {
                              if (const auto file = finished.getResult(); file.existsAsFile())
                                  loadAndRemember (file);
                          });
}

bool SlotRow::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension (audioExtensions.removeCharacters ("*"));
}

void SlotRow::fileDragEnter (const juce::StringArray&, int, int)
{
    dropHover = true;
    repaint();
}

void SlotRow::fileDragExit (const juce::StringArray&)
{
    dropHover = false;
    repaint();
}

void SlotRow::filesDropped (const juce::StringArray& files, int, int)
{
    dropHover = false;
    loadAndRemember (juce::File (files[0]));
    if (onSelect)
        onSelect();
}

} // namespace lucky
