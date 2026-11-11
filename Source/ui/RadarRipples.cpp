#include "RadarRipples.h"

namespace lucky
{

namespace
{
constexpr int framesPerSecond = 60;
constexpr float frameSeconds = 1.0f / framesPerSecond;
constexpr float rippleLifeSeconds = 2.6f;
constexpr float rippleReach = 210.0f;
constexpr float startRadius = 24.0f;
constexpr float onsetJump = 0.1f;
constexpr float minimumRippleGapSeconds = 0.25f;
constexpr size_t maximumRipples = 10;
constexpr float levelRangeLu = 30.0f;
constexpr float levelBelowCentreLu = 24.0f;
} // namespace

RadarRipples::RadarRipples()
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (framesPerSecond);
}

void RadarRipples::ripple (juce::Point<float> from, juce::Colour rippleColour)
{
    if (ripples.size() >= maximumRipples)
        ripples.erase (ripples.begin());
    ripples.push_back ({ from, rippleColour, 1.0f, 0.0f });
    secondsSinceRipple = 0.0f;
}

void RadarRipples::timerCallback()
{
    if (! isShowing() || getLocalBounds().isEmpty())
    {
        listening = false;
        return;
    }

    auto level = 0.0f;
    if (levelSource != nullptr)
    {
        const auto levels = levelSource();
        origin = levels.origin;
        colour = levels.colour;
        const auto centre = std::isfinite (levels.centreLufs) ? levels.centreLufs : -14.0f;
        if (std::isfinite (levels.audibleLufs))
            level = juce::jlimit (0.0f, 1.0f, (levels.audibleLufs - centre + levelBelowCentreLu) / levelRangeLu);
    }

    if (! listening)
    {
        listening = true;
        envelope = level;
        glow = level;
    }

    glow = level > glow ? glow + (level - glow) * 0.35f : glow * 0.95f;
    if (glow < 0.002f)
        glow = 0.0f;

    secondsSinceRipple += frameSeconds;
    if (level - envelope > onsetJump && level > 0.3f && secondsSinceRipple > minimumRippleGapSeconds)
    {
        if (ripples.size() >= maximumRipples)
            ripples.erase (ripples.begin());
        ripples.push_back ({ origin, colour, level, 0.0f });
        secondsSinceRipple = 0.0f;
    }
    envelope = std::max (level, envelope * 0.905f);

    for (auto& r : ripples)
        r.age += frameSeconds;
    ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [] (const Ripple& r) { return r.age >= rippleLifeSeconds; }), ripples.end());

    repaint();
}

void RadarRipples::paint (juce::Graphics& g)
{
    for (const auto& r : ripples)
    {
        const auto progress = r.age / rippleLifeSeconds;
        const auto radius = startRadius + rippleReach * (1.0f - std::pow (1.0f - progress, 2.4f));
        const auto alpha = r.strength * std::pow (1.0f - progress, 1.6f) * 0.5f;
        g.setColour (r.colour.withAlpha (alpha));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (r.origin), 0.8f + 1.6f * (1.0f - progress));
    }

    if (glow > 0.0f)
    {
        const auto outer = 36.0f + 36.0f * glow;
        g.setColour (colour.withAlpha (0.05f + 0.12f * glow));
        g.fillEllipse (juce::Rectangle<float> (outer * 2.0f, outer * 2.0f).withCentre (origin));
        const auto inner = 30.0f + 16.0f * glow;
        g.setColour (colour.withAlpha (0.08f + 0.2f * glow));
        g.fillEllipse (juce::Rectangle<float> (inner * 2.0f, inner * 2.0f).withCentre (origin));
    }
}

} // namespace lucky
