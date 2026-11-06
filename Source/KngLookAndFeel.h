#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** One dark theme for every window in the app. */
class KngLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KngLookAndFeel()
        : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::ColourScheme (
              juce::Colour (0xff1e2124),    // window background
              juce::Colour (0xff2b2f33),    // widget background
              juce::Colour (0xff24282c),    // menu background
              juce::Colour (0xff40464c),    // outline
              juce::Colour (0xffe6e6e6),    // default text
              juce::Colour (0xff33383d),    // default fill
              juce::Colour (0xffffffff),    // highlighted text
              juce::Colour (0xff3a8ee6),    // highlighted fill (accent)
              juce::Colour (0xffe6e6e6)))   // menu text
    {
        const juce::Colour accent (0xff3a8ee6);

        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff1e2124));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff33383d));
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2b2f33));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff40464c));
        setColour (juce::Slider::thumbColourId, accent);
        setColour (juce::Slider::trackColourId, juce::Colour (0xff4a5056));
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff24282c));
        setColour (juce::Slider::rotarySliderFillColourId, accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff40464c));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
        setColour (juce::Label::textColourId, juce::Colour (0xffe6e6e6));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (14.0f, buttonHeight * 0.55f)));
    }
};
