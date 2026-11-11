#include "PluginEditor.h"
#include "ui/SegmentGlyph.h"
#include "ui/Theme.h"

namespace
{
constexpr float minScale = 0.75f;
constexpr float maxScale = 2.0f;
constexpr std::array<float, 4> scalePresets { 1.0f, 1.25f, 1.5f, 1.75f };
constexpr int topBarHeight = 48;
constexpr int logoWidth = 240;
constexpr int iconWidth = 52;
constexpr int gutter = 24;
constexpr int rowHeight = 88;
constexpr int rowGap = 12;
constexpr int sideColumnWidth = 260;
const juce::String minusSign = juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92"));
const juce::String middleDot = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));

juce::String signedDb (float value, const juce::String& unit)
{
    const auto rounded = std::round (value * 10.0f) / 10.0f;
    const auto magnitude = juce::String (std::abs (rounded), 1);
    return (rounded < 0.0f ? minusSign : (rounded > 0.0f ? juce::String ("+") : juce::String())) + magnitude + " " + unit;
}

juce::RangedAudioParameter& parameter (LuckyReferenceProcessor& processor, const juce::String& id)
{
    return *processor.getParameters().getParameter (id);
}

/** Three dots, as on Lucky Seven's menu button. */
std::unique_ptr<juce::Drawable> makeMenuIcon (juce::Colour colour)
{
    juce::Path path;
    for (const auto x : { 4.0f, 11.0f, 18.0f })
        path.addEllipse (x - 1.6f, 9.4f, 3.2f, 3.2f);

    auto drawable = std::make_unique<juce::DrawablePath>();
    drawable->setPath (path);
    drawable->setFill (colour);
    return drawable;
}

/** Top edge of the stage under the slot rows. */
int stageTop()
{
    constexpr auto slots = LuckyReferenceProcessor::numSlots;
    return topBarHeight + gutter * 2 / 3 + slots * rowHeight + (slots - 1) * rowGap + gutter * 2 / 3;
}
} // namespace

LuckyReferenceView::LuckyReferenceView (LuckyReferenceProcessor& processor)
    : owner (processor),
      referenceAttachment (parameter (processor, LuckyReferenceProcessor::referenceId), [this] (float value) { referenceOn = value > 0.5f; syncControls(); }),
      slotAttachment (parameter (processor, LuckyReferenceProcessor::slotId), [this] (float value) { selectedSlot = juce::roundToInt (value); syncControls(); }),
      matchAttachment (*dynamic_cast<juce::AudioParameterBool*> (&parameter (processor, LuckyReferenceProcessor::matchId)), matchButton)
{
    for (int i = 0; i < LuckyReferenceProcessor::numSlots; ++i)
    {
        rows[static_cast<size_t> (i)] = std::make_unique<lucky::SlotRow> (owner, i);
        rows[static_cast<size_t> (i)]->onSelect = [this, i] { slotAttachment.setValueAsCompleteGesture (static_cast<float> (i)); };
        addAndMakeVisible (*rows[static_cast<size_t> (i)]);
    }

    sourceSwitch.onToggle = [this] (bool on) { referenceAttachment.setValueAsCompleteGesture (on ? 1.0f : 0.0f); };
    addAndMakeVisible (sourceSwitch);

    matchButton.setClickingTogglesState (true);
    matchButton.getProperties().set ("outline", true);
    matchButton.getProperties().set ("toggle", true);
    matchButton.setWantsKeyboardFocus (false);
    matchButton.setTooltip ("On: the reference is turned to your mix's loudness, for a fair comparison. "
                            "Off: it plays at its real level, so you can hear the loudness gap.");
    addAndMakeVisible (matchButton);

    const auto idleIcon = makeMenuIcon (lucky::theme::textDim);
    const auto hoverIcon = makeMenuIcon (lucky::theme::text);
    menuButton.setImages (idleIcon.get(), hoverIcon.get());
    menuButton.setEdgeIndent (16);
    menuButton.setWantsKeyboardFocus (false);
    menuButton.onClick = [this] { showMenu(); };
    addAndMakeVisible (menuButton);

    referenceAttachment.sendInitialUpdate();
    slotAttachment.sendInitialUpdate();
    matchAttachment.sendInitialUpdate();
    accent = targetAccent();
    matchButton.setColour (juce::Slider::rotarySliderFillColourId, accent);

    owner.addChangeListener (this);
    startTimerHz (30);
}

LuckyReferenceView::~LuckyReferenceView()
{
    owner.removeChangeListener (this);
}

juce::Colour LuckyReferenceView::targetAccent() const
{
    return referenceOn ? lucky::theme::slotColour (selectedSlot) : lucky::theme::mixColour;
}

void LuckyReferenceView::syncControls()
{
    sourceSwitch.setState (referenceOn, "REF " + juce::String (selectedSlot + 1), lucky::theme::slotColour (selectedSlot));
    for (int i = 0; i < LuckyReferenceProcessor::numSlots; ++i)
        rows[static_cast<size_t> (i)]->setState (i == selectedSlot, referenceOn && i == selectedSlot);
    gainShown = false;
    repaint();
}

void LuckyReferenceView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    gainShown = false;
    for (auto& row : rows)
        row->repaint();
    repaint();
}

void LuckyReferenceView::timerCallback()
{
    sourceSwitch.tick();

    const auto target = targetAccent();
    if (accent != target)
    {
        accent = accent.interpolatedWith (target, 0.25f);
        if (std::abs (accent.getFloatRed() - target.getFloatRed()) + std::abs (accent.getFloatGreen() - target.getFloatGreen())
                + std::abs (accent.getFloatBlue() - target.getFloatBlue()) < 0.01f)
            accent = target;
        matchButton.setColour (juce::Slider::rotarySliderFillColourId, accent);
        matchButton.repaint();
    }

    const auto& engine = owner.getEngine();
    const auto difference = engine.getMixLoudness() - engine.getReferenceLoudness (selectedSlot);
    if (std::isfinite (difference))
    {
        shownGainDb = gainShown ? shownGainDb + (difference - shownGainDb) * 0.2f : difference;
        gainShown = true;
    }

    for (auto& row : rows)
        row->repaint();
    repaint (getLocalBounds().removeFromTop (topBarHeight));
    repaint (getLocalBounds().withTop (stageTop()));
}

void LuckyReferenceView::paint (juce::Graphics& g)
{
    using namespace lucky;
    g.fillAll (theme::background);

    auto topBar = getLocalBounds().removeFromTop (topBarHeight);
    g.setColour (theme::topBar);
    g.fillRect (topBar);

    auto logo = topBar.removeFromLeft (logoWidth).reduced (16, 0).toFloat();
    const auto dim = theme::textDim.withAlpha (0.35f);
    segment::paint (g, logo.removeFromLeft (22.0f).withSizeKeepingCentre (26.0f, 26.0f), segment::letterA, referenceOn ? dim : accent);
    segment::paint (g, logo.removeFromLeft (22.0f).withSizeKeepingCentre (26.0f, 26.0f), segment::letterB, referenceOn ? accent : dim);
    g.setColour (theme::text);
    g.setFont (theme::labelFont (15.0f).withStyle ("Bold"));
    g.drawText ("LUCKY REFERENCE", logo.withTrimmedLeft (12.0f), juce::Justification::centredLeft);

    g.setColour (theme::background);
    for (const auto x : { logoWidth, getWidth() - iconWidth })
        g.fillRect (x, 0, 1, topBarHeight);

    const auto stage = getLocalBounds().withTop (stageTop());
    g.setColour (theme::strip);
    g.fillRect (stage);
    g.setColour (theme::outline);
    g.fillRect (stage.getX(), stage.getY(), stage.getWidth(), 1);

    paintReadout (g, stage.reduced (gutter, 0).removeFromLeft (sideColumnWidth).toFloat());
}

void LuckyReferenceView::paintReadout (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace lucky;
    const auto& engine = owner.getEngine();
    const auto track = owner.getTrack (selectedSlot);
    const auto mix = engine.getMixLoudness();
    const auto reference = engine.getReferenceLoudness (selectedSlot);
    const auto matching = parameter (owner, LuckyReferenceProcessor::matchId).getValue() > 0.5f;

    juce::String headline, detail;
    auto headlineColour = theme::text;
    if (track == nullptr)
    {
        headline = "No reference";
        detail = "Load a reference into slot " + juce::String (selectedSlot + 1);
        headlineColour = theme::textDim;
    }
    else if (! std::isfinite (mix))
    {
        headline = "Measuring...";
        detail = "Play your mix for a few seconds";
        headlineColour = theme::textDim;
    }
    else
    {
        headline = "Ref " + signedDb (shownGainDb, "dB");
        detail = "Mix " + signedDb (mix, "LUFS") + "  " + middleDot + "  Ref " + signedDb (reference, "LUFS");
        if (! matching)
            headlineColour = theme::textDim;
    }

    const auto block = area.withSizeKeepingCentre (area.getWidth(), 60.0f);
    g.setColour (headlineColour);
    g.setFont (theme::labelFont (26.0f).withStyle ("Bold").withKerningFactor (0.02f));
    g.drawText (headline, block.withHeight (34.0f), juce::Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (12.0f));
    g.drawText (detail, block.withTrimmedTop (38.0f), juce::Justification::centredLeft);
}

void LuckyReferenceView::resized()
{
    auto bounds = getLocalBounds();
    auto topBar = bounds.removeFromTop (topBarHeight);
    menuButton.setBounds (topBar.removeFromRight (iconWidth));

    auto area = bounds.withBottom (stageTop()).reduced (gutter, gutter * 2 / 3);
    for (auto& row : rows)
    {
        row->setBounds (area.removeFromTop (rowHeight));
        area.removeFromTop (rowGap);
    }

    auto stage = getLocalBounds().withTop (stageTop()).reduced (gutter, 0);
    stage.removeFromLeft (sideColumnWidth);
    matchButton.setBounds (stage.removeFromRight (sideColumnWidth).removeFromRight (140).withSizeKeepingCentre (140, 34));
    sourceSwitch.setBounds (stage.withSizeKeepingCentre (220, 52));
}

void LuckyReferenceView::showMenu()
{
    juce::PopupMenu sizes;
    const auto scale = getScale != nullptr ? getScale() : 1.0f;
    for (const auto preset : scalePresets)
        sizes.addItem (juce::String (juce::roundToInt (preset * 100.0f)) + "%", true, std::abs (scale - preset) < 0.01f,
                       [this, preset] { if (onScaleRequested) onScaleRequested (preset); });

    juce::PopupMenu menu;
    menu.addSectionHeader (juce::String ("Lucky Reference v") + JucePlugin_VersionString);
    menu.addSubMenu ("Window size", sizes);
    menu.addSeparator();
    menu.addItem ("Put it last on your master, after any limiter", false, false, [] {});
    menu.addItem ("Exports always contain your mix", false, false, [] {});
    menu.addSeparator();
    menu.addItem ("Remove all references", [this]
                  {
                      for (int i = 0; i < LuckyReferenceProcessor::numSlots; ++i)
                          owner.clearSlot (i);
                  });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton));
}

LuckyReferenceEditor::LuckyReferenceEditor (LuckyReferenceProcessor& processorToEdit)
    : AudioProcessorEditor (processorToEdit), view (processorToEdit)
{
    const auto savedScale = juce::jlimit (minScale, maxScale, settings->getUiScale());

    view.getScale = [this] { return currentScale(); };
    view.onScaleRequested = [this] (float scale)
    {
        setSize (juce::roundToInt (LuckyReferenceView::baseWidth * scale), juce::roundToInt (LuckyReferenceView::baseHeight * scale));
    };
    addAndMakeVisible (view);

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (LuckyReferenceView::baseWidth * minScale), juce::roundToInt (LuckyReferenceView::baseHeight * minScale),
                     juce::roundToInt (LuckyReferenceView::baseWidth * maxScale), juce::roundToInt (LuckyReferenceView::baseHeight * maxScale));
    getConstrainer()->setFixedAspectRatio (static_cast<double> (LuckyReferenceView::baseWidth) / LuckyReferenceView::baseHeight);

    setSize (juce::roundToInt (LuckyReferenceView::baseWidth * savedScale), juce::roundToInt (LuckyReferenceView::baseHeight * savedScale));
    sizeRestored = true;
}

float LuckyReferenceEditor::currentScale() const
{
    return static_cast<float> (getWidth()) / static_cast<float> (LuckyReferenceView::baseWidth);
}

void LuckyReferenceEditor::resized()
{
    const auto scale = currentScale();
    view.setBounds (0, 0, LuckyReferenceView::baseWidth, LuckyReferenceView::baseHeight);
    view.setTransform (juce::AffineTransform::scale (scale));

    if (sizeRestored)
        settings->setUiScale (scale);
}
