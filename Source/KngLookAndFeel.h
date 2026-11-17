#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "KngTheme.h"

/** One dark theme for every window in the app: flat buttons, arc knobs, slim sliders. Colors come from KngTheme. */
class KngLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KngLookAndFeel()
        : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::ColourScheme (
              KngTheme::bg,           // window background
              KngTheme::inset,        // widget background
              KngTheme::chrome,       // menu background
              KngTheme::line2,        // outline
              KngTheme::text,         // default text
              KngTheme::btn,          // default fill
              juce::Colours::white,   // highlighted text
              KngTheme::selection,    // highlighted fill
              KngTheme::text))        // menu text
    {
        setColour (juce::ResizableWindow::backgroundColourId, KngTheme::bg);
        setColour (juce::TextButton::buttonColourId, KngTheme::btn);
        setColour (juce::TextButton::buttonOnColourId, KngTheme::accent);
        setColour (juce::TextButton::textColourOffId, KngTheme::text);
        setColour (juce::TextButton::textColourOnId, KngTheme::accentInk);
        setColour (juce::ComboBox::backgroundColourId, KngTheme::btn);
        setColour (juce::ComboBox::outlineColourId, KngTheme::btnLine);
        setColour (juce::ComboBox::textColourId, KngTheme::text);
        setColour (juce::ComboBox::arrowColourId, KngTheme::muted);
        setColour (juce::Slider::thumbColourId, KngTheme::accent);
        setColour (juce::Slider::trackColourId, KngTheme::line2);
        setColour (juce::Slider::backgroundColourId, KngTheme::inset);
        setColour (juce::Slider::rotarySliderFillColourId, KngTheme::accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, KngTheme::line2);
        setColour (juce::Slider::textBoxTextColourId, KngTheme::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ToggleButton::tickColourId, KngTheme::accent);
        setColour (juce::PopupMenu::backgroundColourId, KngTheme::chrome);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, KngTheme::selection);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ScrollBar::thumbColourId, KngTheme::line2);
        setColour (juce::Label::textColourId, KngTheme::text);
        setColour (juce::TooltipWindow::textColourId, KngTheme::text);
    }

    //==============================================================================
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (13.0f, buttonHeight * 0.55f)));
    }

    juce::Font getPopupMenuFont() override               { return juce::Font (juce::FontOptions (14.0f)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }

    //==============================================================================
    // Flat buttons: small radius, thin dark outline, light hover
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                               bool isMouseOver, bool isButtonDown) override
    {
        auto r = button.getLocalBounds().toFloat().reduced (0.5f);
        auto c = background;

        if (isButtonDown)
            c = c.brighter (0.15f);
        else if (isMouseOver)
            c = c.brighter (0.08f);

        g.setColour (c);
        g.fillRoundedRectangle (r, 3.0f);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    //==============================================================================
    // Arc knobs with a pointer
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const float line = juce::jmax (2.0f, radius * 0.16f);
        const float arcRadius = radius - line * 0.5f;

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (KngTheme::line2);
        g.strokePath (track, juce::PathStrokeType (line, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value, juce::PathStrokeType (line, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float body = radius - line * 1.7f;
        g.setColour (KngTheme::btn);
        g.fillEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);

        const juce::Point<float> tip (centre.x + std::sin (angle) * body * 0.8f,
                                      centre.y - std::cos (angle) * body * 0.8f);
        g.setColour (KngTheme::text);
        g.drawLine ({ centre, tip }, 2.0f);
    }

    //==============================================================================
    // Slim sliders with a small square thumb; vertical faders get a wide cap
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearVertical)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                    minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const bool horizontal = style == juce::Slider::LinearHorizontal;
        const float thick = 3.0f;

        juce::Rectangle<float> track = horizontal
            ? juce::Rectangle<float> ((float) x, y + height * 0.5f - thick * 0.5f, (float) width, thick)
            : juce::Rectangle<float> (x + width * 0.5f - thick * 0.5f, (float) y, thick, (float) height);

        g.setColour (KngTheme::line2);
        g.fillRoundedRectangle (track, 1.5f);

        auto fill = horizontal ? track.withRight (sliderPos) : track.withTop (sliderPos);
        g.setColour (slider.findColour (juce::Slider::thumbColourId).withAlpha (0.85f));
        g.fillRoundedRectangle (fill, 1.5f);

        const float thumbW = horizontal ? 8.0f : 26.0f;
        const float thumbH = horizontal ? 14.0f : 12.0f;
        auto thumb = juce::Rectangle<float> (thumbW, thumbH)
                         .withCentre (horizontal ? juce::Point<float> (sliderPos, y + height * 0.5f)
                                                 : juce::Point<float> (x + width * 0.5f, sliderPos));

        g.setColour (KngTheme::text);
        g.fillRoundedRectangle (thumb, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (thumb, 2.0f, 1.0f);

        if (! horizontal)
        {
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawHorizontalLine ((int) thumb.getCentreY(), thumb.getX() + 4.0f, thumb.getRight() - 4.0f);
        }
    }

    //==============================================================================
    // Dark tooltips with a thin border
    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        juce::Rectangle<float> r (0.0f, 0.0f, (float) width, (float) height);

        g.setColour (KngTheme::inset);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (KngTheme::line2);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

        g.setColour (KngTheme::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawFittedText (text, juce::Rectangle<int> (width, height).reduced (8, 4),
                          juce::Justification::centredLeft, 4);
    }
};
