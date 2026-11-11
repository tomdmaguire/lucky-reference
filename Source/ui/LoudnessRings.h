#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace lucky
{

/** Two halos around the A/B switch, one for the mix and one for the reference, whose size follows each signal's
    momentary loudness; matched levels sit together. Switching sends a ripple out in the new source's colour. */
class LoudnessRings final : public juce::Component
{
public:
    LoudnessRings();

    /** The switch's bounds within this component; the halos grow outwards from its outline. */
    void setCore (juce::Rectangle<float> switchBounds);

    /** Sets this frame's momentary loudness of each signal (-infinity when silent) and which one is heard. */
    void setLevels (float mixLufs, float referenceLufs, juce::Colour referenceColour, bool referenceAudible);

    /** Sends a ripple out from the switch. */
    void ripple (juce::Colour colour);

    /** Advances smoothing and ripples by one UI frame. */
    void tick();

    void paint (juce::Graphics&) override;

private:
    struct Ripple
    {
        juce::Colour colour;
        float age = 0.0f;
    };

    float spreadFor (float lufs) const noexcept;
    juce::Path halo (float spread) const;

    juce::Rectangle<float> core;
    float mixSpread = 0.0f, referenceSpread = 0.0f;
    float mixTarget = 0.0f, referenceTarget = 0.0f;
    juce::Colour referenceColour;
    bool referenceAudible = false;
    std::vector<Ripple> ripples;
};

} // namespace lucky
