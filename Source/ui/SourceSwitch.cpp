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
    if (std::abs (knob - target) < 0.001f && std::abs (velocity) < 0.001f)
    {
        knob = target;
        velocity = 0.0f;
        return;
    }

    velocity = (velocity + (target - knob) * 0.22f) * 0.62f;
    knob += velocity;
    repaint();
}

juce::Point<float> SourceSwitch::getKnobCentre() const
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto half = bounds.getWidth() * 0.5f;
    return { bounds.getX() + half * (0.5f + juce::jlimit (0.0f, 1.0f, knob)), bounds.getCentreY() };
}

juce::Point<float> SourceSwitch::getRestingKnobCentre() const
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    return { bounds.getX() + bounds.getWidth() * (reference ? 0.75f : 0.25f), bounds.getCentreY() };
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
    const auto shown = juce::jlimit (0.0f, 1.0f, knob);
    const auto stretch = std::abs (velocity) * half * 0.6f;
    const auto knobBounds = bounds.withWidth (half + stretch).translated (half * knob - (velocity > 0.0f ? stretch : 0.0f), 0.0f)
                                .reduced (3.0f)
                                .constrainedWithin (bounds.reduced (3.0f));
    g.setColour (theme::mixColour.interpolatedWith (colour, shown));
    g.fillRoundedRectangle (knobBounds, knobBounds.getHeight() * 0.5f);

    g.setFont (theme::labelFont (14.0f).withStyle ("Bold"));
    const auto left = bounds.withWidth (half), right = left.translated (half, 0.0f);
    g.setColour (theme::text.interpolatedWith (theme::background, 1.0f - shown));
    g.drawText ("MIX", left, juce::Justification::centred);
    g.setColour (theme::text.interpolatedWith (theme::background, shown));
    g.drawText (label, right, juce::Justification::centred);
}

void SourceSwitch::mouseDown (const juce::MouseEvent&)
{
    if (onToggle)
        onToggle (! reference);
}

} // namespace lucky
