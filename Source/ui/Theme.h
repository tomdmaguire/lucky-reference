#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky::theme
{

inline const juce::Colour background { 0xff1b1c20 };
inline const juce::Colour topBar { 0xff26272c };
inline const juce::Colour strip { 0xff222328 };
inline const juce::Colour panel { 0xff1b1c20 };
inline const juce::Colour panelRaised { 0xff2e3036 };
inline const juce::Colour outline { 0xff34363d };
inline const juce::Colour grid { 0xff26282e };
inline const juce::Colour text { 0xffe6e6e8 };
inline const juce::Colour textDim { 0xff83858e };
inline const juce::Colour accent { 0xffd6b785 };

/** Neutral accent used while the mix is playing. */
inline const juce::Colour mixColour { 0xffc9cad0 };

/** Pastel colour for a reference slot (0, 1 or 2), from the Lucky family palette. */
inline juce::Colour slotColour (int slot)
{
    static const juce::Colour colours[] { accent, juce::Colour (0xffd685be), juce::Colour (0xff85d68f) };
    return colours[static_cast<size_t> (juce::jlimit (0, 2, slot))];
}

/** Heart outline fitted to the given bounds, as in Lucky Seven's favourites. */
inline juce::Path heartPath (juce::Rectangle<float> r)
{
    const auto x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    juce::Path path;
    path.startNewSubPath (x + w * 0.5f, y + h * 0.95f);
    path.cubicTo (x - w * 0.1f, y + h * 0.5f, x + w * 0.1f, y - h * 0.1f, x + w * 0.5f, y + h * 0.25f);
    path.cubicTo (x + w * 0.9f, y - h * 0.1f, x + w * 1.1f, y + h * 0.5f, x + w * 0.5f, y + h * 0.95f);
    path.closeSubPath();
    return path;
}

/** Draws a file-format badge ("WAV", "MP3 · 128 kbps") as an outlined tag. */
inline void paintFormatBadge (juce::Graphics& g, const juce::String& format, juce::Rectangle<float> tag)
{
    g.setColour (outline.brighter (0.3f));
    g.drawRoundedRectangle (tag.reduced (0.5f), 3.0f, 1.0f);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.0f).withStyle ("Bold").withKerningFactor (0.08f));
    g.drawText (format, tag, juce::Justification::centred);
}

/** Small uppercase label font used across the UI. */
inline juce::FontOptions labelFont (float height = 11.0f)
{
    return juce::FontOptions (height).withStyle ("Medium").withKerningFactor (0.08f);
}

} // namespace lucky::theme
