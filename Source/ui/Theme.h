#pragma once

#include <lucky_ui/lucky_ui.h>

/** Lucky Reference's additions to the Lucky family palette in lucky_ui. */
namespace lucky::theme
{

/** Neutral accent used while the mix is playing. */
inline const juce::Colour mixColour { 0xffc9cad0 };

/** Pastel colour for a reference slot (0, 1 or 2), from the Lucky family palette. */
inline juce::Colour slotColour (int slot)
{
    static const juce::Colour colours[] { gold, pink, green };
    return colours[static_cast<size_t> (juce::jlimit (0, 2, slot))];
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

} // namespace lucky::theme
