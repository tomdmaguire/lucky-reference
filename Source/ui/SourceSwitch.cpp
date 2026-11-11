#include "SourceSwitch.h"
#include "Theme.h"

namespace lucky
{

SourceSwitch::SourceSwitch() : colour (theme::slotColour (0))
{
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Mix or reference");
}

void SourceSwitch::setState (bool isReference, const juce::String& referenceLabel, juce::Colour referenceColour)
{
    if (reference == isReference && label == referenceLabel && colour == referenceColour)
        return;

    reference = isReference;
    label = referenceLabel;
    colour = referenceColour;
    repaint();
}

void SourceSwitch::tick()
{
    const auto target = reference ? 1.0f : 0.0f;
    if (std::abs (knob - target) < 0.001f)
    {
        knob = target;
        return;
    }

    knob += (target - knob) * 0.3f;
    repaint();
}

void SourceSwitch::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto radius = bounds.getHeight() * 0.5f;

    g.setColour (theme::background);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (bounds, radius, 1.0f);

    const auto half = bounds.getWidth() * 0.5f;
    const auto knobBounds = bounds.withWidth (half).translated (half * knob, 0.0f).reduced (3.0f);
    g.setColour (theme::mixColour.interpolatedWith (colour, knob));
    g.fillRoundedRectangle (knobBounds, knobBounds.getHeight() * 0.5f);

    g.setFont (theme::labelFont (14.0f).withStyle ("Bold"));
    const auto left = bounds.withWidth (half), right = left.translated (half, 0.0f);
    g.setColour (theme::text.interpolatedWith (theme::background, 1.0f - knob));
    g.drawText ("MIX", left, juce::Justification::centred);
    g.setColour (theme::text.interpolatedWith (theme::background, knob));
    g.drawText (label, right, juce::Justification::centred);
}

void SourceSwitch::mouseDown (const juce::MouseEvent&)
{
    if (onToggle)
        onToggle (! reference);
}

} // namespace lucky
