#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** One dark theme for every window in the app: flat rounded buttons, arc knobs, slim sliders. */
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
        setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
    }

    //==============================================================================
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (14.0f, buttonHeight * 0.55f)));
    }

    juce::Font getPopupMenuFont() override               { return juce::Font (juce::FontOptions (14.0f)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }

    //==============================================================================
    // Flat rounded buttons with a soft hover
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                               bool isMouseOver, bool isButtonDown) override
    {
        auto r = button.getLocalBounds().toFloat().reduced (0.5f);
        auto c = background;

        if (isButtonDown)
            c = c.brighter (0.18f);
        else if (isMouseOver)
            c = c.brighter (0.08f);

        g.setColour (c);
        g.fillRoundedRectangle (r, 5.0f);

        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
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
        g.setColour (juce::Colour (0xff40464c));
        g.strokePath (track, juce::PathStrokeType (line, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value, juce::PathStrokeType (line, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float body = radius - line * 1.7f;
        g.setColour (juce::Colour (0xff2b2f33));
        g.fillEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);

        const juce::Point<float> tip (centre.x + std::sin (angle) * body * 0.8f,
                                      centre.y - std::cos (angle) * body * 0.8f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawLine ({ centre, tip }, 2.0f);
    }

    //==============================================================================
    // Slim sliders with a solid thumb; vertical faders get a wide cap
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
        const float thick = 4.0f;

        juce::Rectangle<float> track = horizontal
            ? juce::Rectangle<float> ((float) x, y + height * 0.5f - thick * 0.5f, (float) width, thick)
            : juce::Rectangle<float> (x + width * 0.5f - thick * 0.5f, (float) y, thick, (float) height);

        g.setColour (juce::Colour (0xff3a3f45));
        g.fillRoundedRectangle (track, 2.0f);

        auto fill = horizontal ? track.withRight (sliderPos) : track.withTop (sliderPos);
        g.setColour (slider.findColour (juce::Slider::thumbColourId).withAlpha (0.85f));
        g.fillRoundedRectangle (fill, 2.0f);

        const float thumbW = horizontal ? 12.0f : 26.0f;
        const float thumbH = horizontal ? 16.0f : 12.0f;
        auto thumb = juce::Rectangle<float> (thumbW, thumbH)
                         .withCentre (horizontal ? juce::Point<float> (sliderPos, y + height * 0.5f)
                                                 : juce::Point<float> (x + width * 0.5f, sliderPos));

        g.setColour (juce::Colour (0xffe6e6e6));
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (thumb, 3.0f, 1.0f);

        if (! horizontal)
        {
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawHorizontalLine ((int) thumb.getCentreY(), thumb.getX() + 4.0f, thumb.getRight() - 4.0f);
        }
    }

    //==============================================================================
    // Dark tooltips with an accent border
    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        juce::Rectangle<float> r (0.0f, 0.0f, (float) width, (float) height);

        g.setColour (juce::Colour (0xff111315));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (accent.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawFittedText (text, juce::Rectangle<int> (width, height).reduced (8, 4),
                          juce::Justification::centredLeft, 4);
    }

private:
    const juce::Colour accent { 0xff3a8ee6 };
};
