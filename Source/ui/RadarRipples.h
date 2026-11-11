#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <limits>
#include <vector>

namespace lucky
{

/** Lucky Seven's map ripples applied to the A/B switch: every hit in the signal being heard sends a circle out from the
    switch's knob that slowly grows and fades, so several faint circles overlap like a radar, and a soft glow behind
    the knob follows the level. The ripples come from wherever the knob is, so they slide across when you switch. */
class RadarRipples final : public juce::Component,
                           private juce::Timer
{
public:
    /** What the radar shows on one frame. */
    struct Levels
    {
        float audibleLufs = -std::numeric_limits<float>::infinity();
        float centreLufs = -14.0f;
        juce::Colour colour;
        juce::Point<float> origin;
    };

    RadarRipples();

    /** Called every frame for the level, colour and origin (in this component's coordinates). */
    std::function<Levels()> levelSource;

    /** Sends a strong ripple out from a point in this component, as when switching. */
    void ripple (juce::Point<float> from, juce::Colour rippleColour);

    void paint (juce::Graphics&) override;

private:
    struct Ripple
    {
        juce::Point<float> origin;
        juce::Colour colour;
        float strength = 1.0f;
        float age = 0.0f;
    };

    void timerCallback() override;

    std::vector<Ripple> ripples;
    juce::Point<float> origin;
    juce::Colour colour;
    float glow = 0.0f, envelope = 0.0f, secondsSinceRipple = 1.0f;
    /** False until the first frame on screen, which only sets the envelope, so opening mid-song isn't heard as a hit. */
    bool listening = false;
};

} // namespace lucky
