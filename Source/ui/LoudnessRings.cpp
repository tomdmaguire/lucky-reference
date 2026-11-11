#include "LoudnessRings.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr int framesPerSecond = 60;
constexpr float belowCentreLu = 9.0f;
constexpr float aboveCentreLu = 13.0f;
constexpr float minimumSpread = 5.0f;
constexpr float rippleFrames = 90.0f;
constexpr float hitThresholdLu = 4.5f;
constexpr int minimumFramesBetweenHits = 30;
constexpr size_t maximumRipples = 12;

/** Eases towards a target: gently up, and back down over about half a second, so the halos drift with the music. */
float follow (float current, float target) noexcept
{
    return current + (target - current) * (target > current ? 0.12f : 0.04f);
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

float LoudnessRings::HitDetector::update (float lufs) noexcept
{
    ++framesSinceHit;
    if (! std::isfinite (lufs))
    {
        primed = false;
        return 0.0f;
    }

    if (! primed)
    {
        average = lufs;
        primed = true;
        return 0.0f;
    }

    const auto rise = lufs - average;
    average += (lufs - average) * 0.06f;
    if (rise < hitThresholdLu || framesSinceHit < minimumFramesBetweenHits)
        return 0.0f;

    framesSinceHit = 0;
    return rise;
}

void LoudnessRings::addRipple (juce::Colour colour, float startSpread, float strength)
{
    if (ripples.size() >= maximumRipples)
        ripples.erase (ripples.begin());
    ripples.push_back ({ colour, startSpread, strength, 0.0f });
}

void LoudnessRings::ripple (juce::Colour colour)
{
    addRipple (colour, 4.0f, 1.0f);
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

        mixSpread = follow (mixSpread, mixTarget);
        referenceSpread = follow (referenceSpread, referenceTarget);

        const auto strengthOf = [] (float rise) { return juce::jlimit (0.3f, 0.6f, rise / 12.0f); };
        const auto mixRise = mixHits.update (levels.mixLufs);
        const auto referenceRise = referenceHits.update (levels.referenceLufs);
        if (! referenceAudible && mixRise > 0.0f)
            addRipple (theme::mixColour, mixSpread, strengthOf (mixRise));
        if (referenceAudible && referenceRise > 0.0f)
            addRipple (referenceColour, referenceSpread, strengthOf (referenceRise));
    }
    else
    {
        mixSpread = follow (mixSpread, mixTarget);
        referenceSpread = follow (referenceSpread, referenceTarget);
    }

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
        const auto spread = r.startSpread + eased * (maximumSpread + 2.0f - r.startSpread);
        g.setColour (r.colour.withAlpha (0.5f * r.strength * (1.0f - r.age)));
        g.strokePath (halo (spread), juce::PathStrokeType (1.0f));
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
