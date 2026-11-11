#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <limits>
#include <vector>

namespace lucky
{

/** Two halos around the A/B switch, one for the mix and one for the reference, that jump with each signal's fast
    K-weighted level like a meter, scaled around the mix's current loudness so matched signals move about the same size.
    Switching sends a ripple out in the new source's colour. */
class LoudnessRings final : public juce::Component,
                            private juce::Timer
{
public:
    /** What the rings show on one frame. */
    struct Levels
    {
        float mixLufs = -std::numeric_limits<float>::infinity();
        float referenceLufs = -std::numeric_limits<float>::infinity();
        float centreLufs = -14.0f;
        juce::Colour referenceColour;
        bool referenceAudible = false;
    };

    LoudnessRings();

    /** Called every frame for the levels to show. */
    std::function<Levels()> levelSource;

    /** The switch's bounds within this component; the halos grow outwards from its outline. */
    void setCore (juce::Rectangle<float> switchBounds);

    /** Sends a ripple out from the switch. */
    void ripple (juce::Colour colour);

    void paint (juce::Graphics&) override;

private:
    struct Ripple
    {
        juce::Colour colour;
        float age = 0.0f;
    };

    void timerCallback() override;
    float spreadFor (float lufs, float centreLufs) const noexcept;
    juce::Path halo (float spread) const;

    juce::Rectangle<float> core;
    float maximumSpread = 60.0f;
    float mixSpread = 0.0f, referenceSpread = 0.0f;
    float mixTarget = 0.0f, referenceTarget = 0.0f;
    juce::Colour referenceColour;
    bool referenceAudible = false;
    std::vector<Ripple> ripples;
};

} // namespace lucky
