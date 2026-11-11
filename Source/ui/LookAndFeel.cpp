#include "LookAndFeel.h"
#include "Theme.h"
#include <BinaryData.h>

namespace lucky
{

LuckyLookAndFeel::LuckyLookAndFeel()
    : regular (juce::Typeface::createSystemTypefaceFor (LuckyReferenceFonts::BarlowRegular_ttf, LuckyReferenceFonts::BarlowRegular_ttfSize)),
      medium (juce::Typeface::createSystemTypefaceFor (LuckyReferenceFonts::BarlowMedium_ttf, LuckyReferenceFonts::BarlowMedium_ttfSize)),
      semiBold (juce::Typeface::createSystemTypefaceFor (LuckyReferenceFonts::BarlowSemiBold_ttf, LuckyReferenceFonts::BarlowSemiBold_ttfSize))
{
    juce::LookAndFeel::setDefaultLookAndFeel (this);
    juce::Typeface::clearTypefaceCache();

    setColour (juce::ResizableWindow::backgroundColourId, theme::background);
    setColour (juce::ListBox::backgroundColourId, theme::background);
    setColour (juce::ListBox::outlineColourId, theme::outline);
    setColour (juce::Slider::rotarySliderFillColourId, theme::accent);
    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::textColourOffId, theme::textDim);
    setColour (juce::TextButton::textColourOnId, theme::text);
    setColour (juce::ScrollBar::thumbColourId, theme::outline);
    setColour (juce::TooltipWindow::backgroundColourId, theme::panelRaised);
    setColour (juce::TooltipWindow::textColourId, theme::text);
    setColour (juce::TooltipWindow::outlineColourId, theme::outline);
}

LuckyLookAndFeel::~LuckyLookAndFeel()
{
    if (&juce::LookAndFeel::getDefaultLookAndFeel() == this)
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    juce::Typeface::clearTypefaceCache();
}

juce::Typeface::Ptr LuckyLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    if (font.getTypefaceName() != juce::Font::getDefaultSansSerifFontName())
        return juce::LookAndFeel_V4::getTypefaceForFont (font);

    if (font.isBold())
        return semiBold;
    if (font.getTypefaceStyle().containsIgnoreCase ("Medium"))
        return medium;
    return regular;
}

void LuckyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool isMouseOver, bool)
{
    if (button.getProperties()["outline"])
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.75f);
        const auto colour = button.findColour (juce::Slider::rotarySliderFillColourId);
        if (button.getProperties()["primary"])
        {
            g.setColour (isMouseOver ? colour.brighter (0.15f) : colour);
            g.fillRoundedRectangle (bounds, 4.0f);
        }
        else if (button.getProperties()["toggle"] && button.getToggleState())
        {
            g.setColour (colour.withAlpha (isMouseOver ? 0.2f : 0.14f));
            g.fillRoundedRectangle (bounds, 4.0f);
            g.setColour (colour.withAlpha (0.8f));
            g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
        }
        else
        {
            g.setColour (isMouseOver ? theme::text.withAlpha (0.06f) : juce::Colours::transparentBlack);
            g.fillRoundedRectangle (bounds, 4.0f);
            g.setColour (theme::outline.brighter (isMouseOver ? 0.4f : 0.15f));
            g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
        }
        return;
    }

    if (button.getProperties()["round"])
    {
        if (isMouseOver)
        {
            g.setColour (theme::text.withAlpha (0.08f));
            g.fillEllipse (button.getLocalBounds().toFloat().reduced (2.0f));
        }
        return;
    }

    if (button.getToggleState())
    {
        g.setColour (button.findColour (juce::Slider::rotarySliderFillColourId));
        g.fillRect (button.getLocalBounds().removeFromBottom (2));
    }
    else if (isMouseOver)
    {
        g.setColour (theme::text.withAlpha (0.04f));
        g.fillRect (button.getLocalBounds());
    }
}

juce::Font LuckyLookAndFeel::getTextButtonFont (juce::TextButton& button, int)
{
    const auto& properties = button.getProperties();
    return theme::labelFont (properties["small"] || properties["outline"] ? 11.0f : 13.0f);
}

void LuckyLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool isMouseOver, bool isButtonDown)
{
    if (! button.getProperties()["outline"])
    {
        juce::LookAndFeel_V4::drawButtonText (g, button, isMouseOver, isButtonDown);
        return;
    }

    g.setFont (getTextButtonFont (button, button.getHeight()));
    const auto& properties = button.getProperties();
    auto area = button.getLocalBounds();

    if (properties["toggle"])
    {
        const auto on = button.getToggleState();
        const auto colour = button.findColour (juce::Slider::rotarySliderFillColourId);
        const auto textWidth = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), button.getButtonText());
        auto content = area.withSizeKeepingCentre (textWidth + 14, area.getHeight());
        const auto dot = content.removeFromLeft (8).toFloat().withSizeKeepingCentre (6.0f, 6.0f);
        g.setColour (on ? colour : theme::textDim.withAlpha (0.5f));
        g.fillEllipse (dot);
        g.setColour (on ? theme::text : theme::textDim);
        g.drawText (button.getButtonText(), content.withTrimmedLeft (6), juce::Justification::centredLeft);
        return;
    }

    g.setColour (properties["primary"] ? theme::background : theme::text);
    g.drawText (button.getButtonText(), area, juce::Justification::centred);
}

} // namespace lucky
