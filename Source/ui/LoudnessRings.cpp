#include "LoudnessRings.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr float quietLufs = -40.0f;
constexpr float loudLufs = -6.0f;
constexpr float minimumSpread = 8.0f;
constexpr float maximumSpread = 78.0f;
constexpr float rippleFrames = 26.0f;

/** Moves towards a target quickly when rising and slowly when falling, like a meter's ballistics. */
float follow (float current, float target) noexcept
{
    return current + (target - current) * (target > current ? 0.45f : 0.08f);
}
} // namespace

LoudnessRings::LoudnessRings() : referenceColour (theme::slotColour (0))
{
    setInterceptsMouseClicks (false, false);
}

void LoudnessRings::setCore (juce::Rectangle<float> switchBounds)
{
    core = switchBounds;
    repaint();
}

float LoudnessRings::spreadFor (float lufs) const noexcept
{
    if (! std::isfinite (lufs))
        return 0.0f;
    const auto position = juce::jlimit (0.0f, 1.0f, (lufs - quietLufs) / (loudLufs - quietLufs));
    return minimumSpread + position * (maximumSpread - minimumSpread);
}

void LoudnessRings::setLevels (float mixLufs, float referenceLufs, juce::Colour colour, bool audible)
{
    mixTarget = spreadFor (mixLufs);
    referenceTarget = spreadFor (referenceLufs);
    referenceColour = colour;
    referenceAudible = audible;
}

void LoudnessRings::ripple (juce::Colour colour)
{
    ripples.push_back ({ colour, 0.0f });
}

void LoudnessRings::tick()
{
    mixSpread = follow (mixSpread, mixTarget);
    referenceSpread = follow (referenceSpread, referenceTarget);

    for (auto& r : ripples)
        r.age += 1.0f / rippleFrames;
    ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [] (const Ripple& r) { return r.age >= 1.0f; }), ripples.end());
    repaint();
}

juce::Path LoudnessRings::halo (float spread) const
{
    const auto bounds = core.expanded (spread);
    juce::Path path;
    path.addRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
    return path;
}

void LoudnessRings::paint (juce::Graphics& g)
{
    if (core.isEmpty())
        return;

    for (const auto& r : ripples)
    {
        const auto eased = 1.0f - std::pow (1.0f - r.age, 3.0f);
        g.setColour (r.colour.withAlpha (0.55f * (1.0f - r.age)));
        g.strokePath (halo (4.0f + eased * (maximumSpread + 12.0f)), juce::PathStrokeType (2.0f));
    }

    const auto drawHalo = [&] (float spread, juce::Colour colour, bool audible)
    {
        if (spread <= 0.5f)
            return;
        const auto path = halo (spread);
        if (audible)
        {
            g.setColour (colour.withAlpha (0.07f));
            g.fillPath (path);
        }
        g.setColour (colour.withAlpha (audible ? 0.9f : 0.3f));
        g.strokePath (path, juce::PathStrokeType (audible ? 2.0f : 1.5f));
    };

    drawHalo (referenceAudible ? mixSpread : referenceSpread, referenceAudible ? theme::mixColour : referenceColour, false);
    drawHalo (referenceAudible ? referenceSpread : mixSpread, referenceAudible ? referenceColour : theme::mixColour, true);
}

} // namespace lucky
