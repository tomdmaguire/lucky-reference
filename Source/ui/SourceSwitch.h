#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** The A/B switch: a pill with MIX on the left and the selected reference on the right, and a knob that slides between them. */
class SourceSwitch final : public juce::Component
{
public:
    SourceSwitch();

    /** Called when clicked, with the side the switch should move to. */
    std::function<void (bool reference)> onToggle;

    /** Sets which side is on, the reference's label and its colour; the knob animates via tick(). */
    void setState (bool isReference, const juce::String& referenceLabel, juce::Colour referenceColour);

    /** Advances the knob animation by one UI frame. */
    void tick();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    bool reference = false;
    float knob = 0.0f;
    float velocity = 0.0f;
    juce::String label { "REF 1" };
    juce::Colour colour;
};

} // namespace lucky
