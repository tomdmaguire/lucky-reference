#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** Seven-segment glyphs, drawn like the Lucky family logo: lit segments in the colour, unlit ones faintly. */
namespace segment
{
enum : int
{
    top = 1 << 0,
    topRight = 1 << 1,
    bottomRight = 1 << 2,
    bottom = 1 << 3,
    bottomLeft = 1 << 4,
    topLeft = 1 << 5,
    middle = 1 << 6,
};

inline constexpr int letterA = top | topRight | bottomRight | bottomLeft | topLeft | middle;
inline constexpr int letterB = bottomRight | bottom | bottomLeft | topLeft | middle;

/** Draws a glyph in a square, with unlit segments at unlitAlpha of the colour. */
inline void paint (juce::Graphics& g, juce::Rectangle<float> bounds, int lit, juce::Colour colour, float unlitAlpha = 0.12f)
{
    const auto scale = bounds.getWidth() / 64.0f;
    const auto point = [&] (float x, float y) { return bounds.getTopLeft() + juce::Point<float> (x, y) * scale; };
    const juce::PathStrokeType stroke (5.5f * scale, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded);

    struct Segment { int bit; float x1, y1, x2, y2; };
    static constexpr Segment segments[] {
        { top, 22, 14, 42, 14 },       { topRight, 45, 17, 45, 30 }, { bottomRight, 45, 35, 45, 48 }, { bottom, 22, 51, 42, 51 },
        { bottomLeft, 19, 35, 19, 48 }, { topLeft, 19, 17, 19, 30 }, { middle, 22, 32.5f, 42, 32.5f },
    };

    for (const auto& s : segments)
    {
        juce::Path path;
        path.startNewSubPath (point (s.x1, s.y1));
        path.lineTo (point (s.x2, s.y2));
        g.setColour ((lit & s.bit) != 0 ? colour : colour.withAlpha (unlitAlpha));
        g.strokePath (path, stroke);
    }
}
} // namespace segment

} // namespace lucky
