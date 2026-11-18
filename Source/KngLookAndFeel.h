#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "KngTheme.h"
#include "KngFonts.h"
#include "KngLogo.h"

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

    // Every font without its own typeface (the whole main UI) uses Inter.
    // Plugin faces set their own typeface and are not affected.
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override
    {
        if (auto tf = KngFonts::get (font.isBold() ? KngFonts::interSemiBold : KngFonts::interRegular))
            return tf;

        return juce::LookAndFeel_V4::getTypefaceForFont (font);
    }

    //==============================================================================
    // The window's own title bar: dark, with the crown logo and the app name
    void drawDocumentWindowTitleBar (juce::DocumentWindow& window, juce::Graphics& g, int w, int h,
                                     int, int, const juce::Image*, bool) override
    {
        g.fillAll (KngTheme::menu);
        g.setColour (KngTheme::line);
        g.drawHorizontalLine (h - 1, 0.0f, (float) w);

        float x = 12.0f;
        const auto& logo = KngLogo::keyed();

        if (logo.isValid())
        {
            // Just the crown, cropped out of the full logo
            const int iconH = h - 10;
            const int iconW = juce::roundToInt ((float) iconH * 400.0f / 295.0f);
            g.setOpacity (1.0f);
            g.drawImage (logo, (int) x, (h - iconH) / 2, iconW, iconH, 190, 55, 400, 295);
            x += (float) iconW + 8.0f;
        }

        g.setColour (KngTheme::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (window.getName(), juce::Rectangle<float> (x, 0.0f, (float) w - x, (float) h),
                    juce::Justification::centredLeft, true);
    }

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
        g.fillRoundedRectangle (r, KngTheme::radius);

        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.drawRoundedRectangle (r, KngTheme::radius, 1.0f);
    }

    //==============================================================================
    // Dropdown boxes: rounded, with a small chevron
    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                       juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, KngTheme::radius);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, KngTheme::radius, 1.0f);

        const float cx = (float) width - 14.0f;
        const float cy = (float) height * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath (cx - 3.5f, cy - 1.5f);
        arrow.lineTo (cx, cy + 2.0f);
        arrow.lineTo (cx + 3.5f, cy - 1.5f);

        g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.3f));
        g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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

        // Pan-style knobs (symmetric range) fill from the middle
        const bool bipolar = slider.getMinimum() == -slider.getMaximum();
        const float fromAngle = bipolar ? startAngle + (endAngle - startAngle) * 0.5f : startAngle;

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, fromAngle, angle, true);
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
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (thumb, 3.0f, 1.0f);

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
        g.fillRoundedRectangle (r, KngTheme::radius);
        g.setColour (KngTheme::line2);
        g.drawRoundedRectangle (r.reduced (0.5f), KngTheme::radius, 1.0f);

        g.setColour (KngTheme::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawFittedText (text, juce::Rectangle<int> (width, height).reduced (8, 4),
                          juce::Justification::centredLeft, 4);
    }
};
