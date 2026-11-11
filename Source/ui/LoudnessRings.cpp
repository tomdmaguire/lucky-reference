#include "LoudnessRings.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr int framesPerSecond = 60;
constexpr float belowCentreLu = 12.0f;
constexpr float aboveCentreLu = 6.0f;
constexpr float minimumSpread = 6.0f;
constexpr float rippleFrames = 50.0f;

/** Rises almost at once and falls back over about 150 ms, like a meter's ballistics. */
float follow (float current, float target) noexcept
{
    return current + (target - current) * (target > current ? 0.65f : 0.14f);
}
} // namespace

LoudnessRings::LoudnessRings() : referenceColour (theme::slotColour (0))
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (framesPerSecond);
}

void LoudnessRings::setCore (juce::Rectangle<float> switchBounds)
{
    core = switchBounds;
    const auto room = std::min ({ core.getY(), static_cast<float> (getHeight()) - core.getBottom(),
                                  core.getX(), static_cast<float> (getWidth()) - core.getRight() });
    maximumSpread = std::max (minimumSpread + 1.0f, room - 6.0f);
    repaint();
}

float LoudnessRings::spreadFor (float lufs, float centreLufs) const noexcept
{
    if (! std::isfinite (lufs))
        return 0.0f;
    const auto position = juce::jlimit (0.0f, 1.0f, (lufs - centreLufs + belowCentreLu) / (belowCentreLu + aboveCentreLu));
    return minimumSpread + position * (maximumSpread - minimumSpread);
}

void LoudnessRings::ripple (juce::Colour colour)
{
    ripples.push_back ({ colour, 0.0f });
}

void LoudnessRings::timerCallback()
{
    if (levelSource != nullptr)
    {
        const auto levels = levelSource();
        const auto centre = std::isfinite (levels.centreLufs) ? levels.centreLufs : -14.0f;
        mixTarget = spreadFor (levels.mixLufs, centre);
        referenceTarget = spreadFor (levels.referenceLufs, centre);
        referenceColour = levels.referenceColour;
        referenceAudible = levels.referenceAudible;
    }

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
        g.strokePath (halo (4.0f + eased * (maximumSpread + 2.0f)), juce::PathStrokeType (2.0f));
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
        g.setColour (colour.withAlpha (audible ? 0.9f : 0.45f));
        g.strokePath (path, juce::PathStrokeType (audible ? 2.0f : 1.5f));
    };

    drawHalo (referenceAudible ? mixSpread : referenceSpread, referenceAudible ? theme::mixColour : referenceColour, false);
    drawHalo (referenceAudible ? referenceSpread : mixSpread, referenceAudible ? referenceColour : theme::mixColour, true);
}

} // namespace lucky
