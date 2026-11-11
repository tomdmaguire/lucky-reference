#include "PluginEditor.h"
#include "ui/SegmentGlyph.h"
#include "ui/Theme.h"

namespace
{
constexpr int topBarHeight = 48;
constexpr int bottomHeight = 104;
constexpr int gutter = 12;
const juce::String minusSign = juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92"));

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
} // namespace

LuckyReferenceEditor::LuckyReferenceEditor (LuckyReferenceProcessor& processorToEdit)
    : AudioProcessorEditor (processorToEdit),
      owner (processorToEdit),
      referenceAttachment (parameter (processorToEdit, LuckyReferenceProcessor::referenceId), [this] (float value) { referenceOn = value > 0.5f; syncControls(); }),
      slotAttachment (parameter (processorToEdit, LuckyReferenceProcessor::slotId), [this] (float value) { selectedSlot = juce::roundToInt (value); syncControls(); }),
      matchAttachment (*dynamic_cast<juce::AudioParameterBool*> (&parameter (processorToEdit, LuckyReferenceProcessor::matchId)), matchButton)
{
    setWantsKeyboardFocus (false);

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
    matchButton.setTooltip ("Turn the reference to your mix's loudness");
    addAndMakeVisible (matchButton);

    menuButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x8b\xaf")));
    menuButton.getProperties().set ("round", true);
    menuButton.setWantsKeyboardFocus (false);
    menuButton.onClick = [this] { showMenu(); };
    addAndMakeVisible (menuButton);

    referenceAttachment.sendInitialUpdate();
    slotAttachment.sendInitialUpdate();
    matchAttachment.sendInitialUpdate();
    accent = targetAccent();
    matchButton.setColour (juce::Slider::rotarySliderFillColourId, accent);

    owner.addChangeListener (this);
    setResizable (true, true);
    setResizeLimits (600, 380, 1400, 760);
    setSize (720, 420);
    startTimerHz (30);
}

LuckyReferenceEditor::~LuckyReferenceEditor()
{
    owner.removeChangeListener (this);
}

juce::Colour LuckyReferenceEditor::targetAccent() const
{
    return referenceOn ? lucky::theme::slotColour (selectedSlot) : lucky::theme::mixColour;
}

void LuckyReferenceEditor::syncControls()
{
    sourceSwitch.setState (referenceOn, "REF " + juce::String (selectedSlot + 1), lucky::theme::slotColour (selectedSlot));
    for (int i = 0; i < LuckyReferenceProcessor::numSlots; ++i)
        rows[static_cast<size_t> (i)]->setState (i == selectedSlot, referenceOn && i == selectedSlot);
    gainShown = false;
    repaint();
}

void LuckyReferenceEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    gainShown = false;
    for (auto& row : rows)
        row->repaint();
    repaint();
}

void LuckyReferenceEditor::timerCallback()
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
    repaint (getLocalBounds().removeFromBottom (bottomHeight));
}

void LuckyReferenceEditor::paint (juce::Graphics& g)
{
    using namespace lucky;
    g.fillAll (theme::background);

    auto topBar = getLocalBounds().removeFromTop (topBarHeight).toFloat();
    g.setColour (theme::topBar);
    g.fillRect (topBar);

    auto logo = topBar.reduced (static_cast<float> (gutter), 0.0f);
    const auto glyphA = logo.removeFromLeft (22.0f).withSizeKeepingCentre (26.0f, 26.0f);
    const auto glyphB = logo.removeFromLeft (22.0f).withSizeKeepingCentre (26.0f, 26.0f);
    const auto dim = theme::textDim.withAlpha (0.35f);
    segment::paint (g, glyphA, segment::letterA, referenceOn ? dim : accent);
    segment::paint (g, glyphB, segment::letterB, referenceOn ? accent : dim);

    g.setColour (theme::text);
    g.setFont (theme::labelFont (15.0f).withStyle ("Bold"));
    g.drawText ("LUCKY REFERENCE", logo.withTrimmedLeft (12.0f), juce::Justification::centredLeft);

    auto bottom = getLocalBounds().removeFromBottom (bottomHeight).toFloat();
    g.setColour (theme::strip);
    g.fillRect (bottom);
    paintReadout (g, bottom.withTrimmedLeft (static_cast<float> (gutter * 2 + sourceSwitch.getWidth()))
                          .withTrimmedRight (static_cast<float> (gutter * 2 + matchButton.getWidth())));
}

void LuckyReferenceEditor::paintReadout (juce::Graphics& g, juce::Rectangle<float> area)
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
        detail = "Mix " + signedDb (mix, "LUFS") + "  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  Ref " + signedDb (reference, "LUFS");
        if (! matching)
        {
            detail << "  " << juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) << "  not applied";
            headlineColour = theme::textDim;
        }
    }

    const auto block = area.withSizeKeepingCentre (area.getWidth(), 56.0f);
    g.setColour (headlineColour);
    g.setFont (theme::labelFont (28.0f).withStyle ("Bold").withKerningFactor (0.02f));
    g.drawText (headline, block.withHeight (34.0f), juce::Justification::centred);
    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (12.0f));
    g.drawText (detail, block.withTrimmedTop (36.0f), juce::Justification::centred);
}

void LuckyReferenceEditor::resized()
{
    auto bounds = getLocalBounds();
    auto topBar = bounds.removeFromTop (topBarHeight);
    menuButton.setBounds (topBar.removeFromRight (topBarHeight).reduced (8));

    auto bottom = bounds.removeFromBottom (bottomHeight).reduced (gutter * 2, 0);
    sourceSwitch.setBounds (bottom.removeFromLeft (220).withSizeKeepingCentre (220, 52));
    matchButton.setBounds (bottom.removeFromRight (96).withSizeKeepingCentre (96, 32));

    auto area = bounds.reduced (gutter);
    const auto rowHeight = (area.getHeight() - gutter * 2) / LuckyReferenceProcessor::numSlots;
    for (auto& row : rows)
    {
        row->setBounds (area.removeFromTop (rowHeight));
        area.removeFromTop (gutter);
    }
}

void LuckyReferenceEditor::showMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Lucky Reference " JucePlugin_VersionString);
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
