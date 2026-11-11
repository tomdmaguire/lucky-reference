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

/** Small uppercase label font used across the UI. */
inline juce::FontOptions labelFont (float height = 11.0f)
{
    return juce::FontOptions (height).withStyle ("Medium").withKerningFactor (0.08f);
}

} // namespace lucky::theme
