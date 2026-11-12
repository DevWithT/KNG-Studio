#include "KngFace.h"

namespace te = tracktion;

juce::String kngFormat (const KngParamDef& d, float v)
{
    const juce::String unit (d.unit != nullptr ? d.unit : "");

    if (unit == "%")
        return juce::String (juce::roundToInt (v * 100.0f)) + "%";

    if (unit == "Hz")
        return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";

    if (unit == "lfo")
        return juce::String (v, 2) + " Hz";

    if (unit == "ms")
        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms";

    if (unit == "sec")
        return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s";

    if (unit == "dB")
        return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";

    if (unit == "st")
        return (v > 0.5f ? "+" : "") + juce::String (juce::roundToInt (v)) + " st";

    if (unit == "ct")
        return juce::String (juce::roundToInt (v)) + " ct";

    return juce::String (v, d.step >= 1.0f ? 0 : 2);
}

static void beginUndoStep (te::Plugin* plugin)
{
    if (plugin != nullptr)
        if (auto* um = plugin->getUndoManager())
            um->beginNewTransaction();
}

//==============================================================================
KngFaceLook::KngFaceLook (const KngFaceStyle& styleToUse)
    : style (styleToUse)
{
    setColour (juce::Slider::rotarySliderFillColourId, style.accent);
    setColour (juce::ComboBox::textColourId, style.value);
    setColour (juce::ComboBox::backgroundColourId, style.panel);
    setColour (juce::ComboBox::outlineColourId, style.outline);
    setColour (juce::ComboBox::arrowColourId, style.label);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff151617));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe6e6e6));
    setColour (juce::PopupMenu::headerTextColourId, style.label);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, style.accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, style.panel);
    setColour (juce::TextButton::textColourOffId, style.label);
    setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
    setColour (juce::Label::textColourId, style.value);
}

void KngFaceLook::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                    float startAngle, float endAngle, juce::Slider& slider)
{
    const auto b = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const float size = juce::jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float ring = juce::jmax (3.0f, size * 0.055f);
    const float r = size * 0.5f - ring;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto arcColour = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const juce::PathStrokeType stroke (ring, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    if (r <= 2.0f)
        return;

    // The ring: a dim track with the value glowing over it
    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
    g.setColour (style.track);
    g.strokePath (track, stroke);

    juce::Path value;
    value.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, angle, true);
    g.setColour (arcColour.withAlpha (0.22f));
    g.strokePath (value, juce::PathStrokeType (ring * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (arcColour);
    g.strokePath (value, stroke);

    // The body: a shaded cap with a soft shadow under it
    const float body = r - ring * 1.6f;

    if (body <= 1.0f)
        return;

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - body, c.y - body + body * 0.08f, body * 2.0f, body * 2.0f);

    juce::ColourGradient shade (style.bodyLight, c.x - body * 0.3f, c.y - body * 0.45f,
                                style.bodyDark, c.x + body * 0.7f, c.y + body * 0.8f, true);
    g.setGradientFill (shade);
    g.fillEllipse (c.x - body, c.y - body, body * 2.0f, body * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (c.x - body + 0.5f, c.y - body + 0.5f, body * 2.0f - 1.0f, body * 2.0f - 1.0f, 1.0f);

    // Pointer
    juce::Path pointer;
    pointer.startNewSubPath (c.x + std::sin (angle) * body * 0.3f, c.y - std::cos (angle) * body * 0.3f);
    pointer.lineTo (c.x + std::sin (angle) * body * 0.82f, c.y - std::cos (angle) * body * 0.82f);
    g.setColour (style.pointer);
    g.strokePath (pointer, juce::PathStrokeType (juce::jmax (2.0f, body * 0.09f), juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

void KngFaceLook::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    const float cx = (float) width - 16.0f, cy = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void KngFaceLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

juce::Font KngFaceLook::getComboBoxFont (juce::ComboBox&)
{
    return KngFonts::font (style.menuFace, 14.0f);
}

juce::Font KngFaceLook::getPopupMenuFont()
{
    return KngFonts::font (style.menuFace, 14.0f);
}

void KngFaceLook::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                                        bool isMouseOver, bool isButtonDown)
{
    const auto r = button.getLocalBounds().toFloat().reduced (0.5f);
    auto c = background;

    if (isButtonDown)
        c = c.brighter (0.2f);
    else if (isMouseOver)
        c = c.brighter (0.1f);

    g.setColour (c);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (style.outline);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);
}

juce::Font KngFaceLook::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return KngFonts::font (style.labelFace, juce::jmin (15.0f, buttonHeight * 0.5f), 0.1f);
}

//==============================================================================
KngFaceKnob::KngFaceKnob (KngFxBase& p, int paramIndex, const KngFaceStyle& styleToUse, const juce::String& labelText)
    : KngFaceKnob (p, *p.getParam (paramIndex), p.getDef (paramIndex), styleToUse, labelText)
{
}

KngFaceKnob::KngFaceKnob (te::Plugin& p, te::AutomatableParameter& parameter, const KngParamDef& defToUse,
                          const KngFaceStyle& styleToUse, const juce::String& labelText)
    : pluginRef (&p), param (&parameter), def (defToUse), style (styleToUse),
      label (labelText.isNotEmpty() ? labelText : juce::String (defToUse.name))
{
    labelColour = style.label;

    addAndMakeVisible (knob);
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    knob.setRange (def.min, def.max, def.step);

    if (def.mid > def.min && def.mid < def.max)
        knob.setSkewFactorFromMidPoint (def.mid);

    knob.setDoubleClickReturnValue (true, def.def);
    knob.setValue (param->getCurrentValue(), juce::dontSendNotification);
    knob.setTooltip (label + " (double-click to reset)");

    knob.onDragStart = [this] { beginUndoStep (pluginRef.get()); };

    knob.onValueChange = [this]
    {
        param->setParameter ((float) knob.getValue(), juce::sendNotification);
        repaint();
    };

    startTimerHz (20);
}

KngFaceKnob::~KngFaceKnob()
{
    stopTimer();
}

void KngFaceKnob::timerCallback()
{
    // Follow automation, presets and undo
    const float v = param->getCurrentValue();

    if (! knob.isMouseButtonDown() && std::abs (v - (float) knob.getValue()) > 1.0e-6f)
    {
        knob.setValue (v, juce::dontSendNotification);
        repaint();
    }
}

void KngFaceKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom ((int) (labelSize + valueSize + 8.0f));
    const int size = juce::jmin (r.getWidth(), r.getHeight());
    knob.setBounds (r.withSizeKeepingCentre (size, size));
}

void KngFaceKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto valueArea = r.removeFromBottom (valueSize + 3.0f);
    auto labelArea = r.removeFromBottom (labelSize + 5.0f);

    g.setFont (KngFonts::font (style.labelFace, labelSize, 0.12f));
    g.setColour (labelColour);
    g.drawText (style.upperCaseLabels ? label.toUpperCase() : label, labelArea, juce::Justification::centredBottom, false);

    g.setFont (KngFonts::font (style.valueFace, valueSize));
    g.setColour (style.value);
    const float v = (float) knob.getValue();
    g.drawText (formatter ? formatter (v) : kngFormat (def, v), valueArea, juce::Justification::centred, false);
}

//==============================================================================
KngFaceToggle::KngFaceToggle (KngFxBase& p, int paramIndex, const KngFaceStyle& styleToUse, const juce::String& labelText)
    : KngFaceToggle (p, *p.getParam (paramIndex), styleToUse, labelText)
{
}

KngFaceToggle::KngFaceToggle (te::Plugin& p, te::AutomatableParameter& parameter,
                              const KngFaceStyle& styleToUse, const juce::String& labelText)
    : pluginRef (&p), param (&parameter), style (styleToUse), label (labelText)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (10);
}

KngFaceToggle::~KngFaceToggle()
{
    stopTimer();
}

void KngFaceToggle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto labelArea = r.removeFromBottom (18.0f);
    const auto box = r.withSizeKeepingCentre (juce::jmin (64.0f, r.getWidth()), juce::jmin (34.0f, r.getHeight()));
    const bool on = param->getCurrentValue() > 0.5f;

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (box.translated (0.0f, 2.0f), 6.0f);

    juce::ColourGradient shade (style.bodyLight, box.getX(), box.getY(), style.bodyDark, box.getX(), box.getBottom(), false);
    g.setGradientFill (shade);
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (style.outline);
    g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);

    const auto c = box.getCentre();

    if (on)
    {
        g.setColour (style.accent.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (c));
        g.setColour (style.accent);
    }
    else
    {
        g.setColour (juce::Colour (0xff2a2a2a));
    }

    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (c));

    g.setFont (KngFonts::font (style.labelFace, 13.0f, 0.12f));
    g.setColour (style.label);
    g.drawText (style.upperCaseLabels ? label.toUpperCase() : label, labelArea, juce::Justification::centred, false);
}

void KngFaceToggle::mouseDown (const juce::MouseEvent&)
{
    beginUndoStep (pluginRef.get());
    param->setParameter (param->getCurrentValue() > 0.5f ? 0.0f : 1.0f, juce::sendNotification);
    repaint();
}

//==============================================================================
KngFaceChoice::KngFaceChoice (te::Plugin& p, te::AutomatableParameter& parameter, const KngParamDef& defToUse,
                              const KngFaceStyle& styleToUse, const juce::String& shortNames)
    : pluginRef (&p), param (&parameter), def (defToUse), style (styleToUse)
{
    onColour = style.accent;
    names = juce::StringArray::fromTokens (shortNames.isNotEmpty() ? shortNames
                                                                   : juce::String (def.choices != nullptr ? def.choices : ""),
                                           "|", "");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (8);
}

KngFaceChoice::~KngFaceChoice()
{
    stopTimer();
}

int KngFaceChoice::getSelected() const
{
    return juce::roundToInt (param->getCurrentValue() - def.min);
}

void KngFaceChoice::paint (juce::Graphics& g)
{
    const int n = names.size();

    if (n == 0)
        return;

    const float w = (float) getWidth() / (float) n;
    const int selected = getSelected();
    g.setFont (KngFonts::font (KngFonts::mono, juce::jmin (11.0f, (float) getHeight() * 0.6f)));

    for (int i = 0; i < n; ++i)
    {
        const auto cell = juce::Rectangle<float> (i * w, 0.0f, w, (float) getHeight()).reduced (2.0f, 0.0f);
        const bool on = i == selected;

        g.setColour (on ? onColour : style.panel);
        g.fillRoundedRectangle (cell, cell.getHeight() * 0.5f);
        g.setColour (on ? onTextColour : style.label);
        g.drawText (names[i], cell, juce::Justification::centred, false);
    }
}

void KngFaceChoice::mouseDown (const juce::MouseEvent& e)
{
    const int n = names.size();

    if (n == 0 || getWidth() <= 0)
        return;

    const int i = juce::jlimit (0, n - 1, e.x * n / getWidth());
    beginUndoStep (pluginRef.get());
    param->setParameter (def.min + (float) i, juce::sendNotification);
    repaint();
}

//==============================================================================
void KngPopButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto colour = findColour (juce::ComboBox::arrowColourId);

    if (isMouseOver || isButtonDown)
        colour = colour.brighter (0.4f);

    g.setColour (findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    const auto c = r.getCentre();
    juce::Path arrow;
    arrow.startNewSubPath (c.x - 5.0f, c.y + 5.0f);
    arrow.lineTo (c.x + 5.0f, c.y - 5.0f);
    arrow.startNewSubPath (c.x - 1.0f, c.y - 5.0f);
    arrow.lineTo (c.x + 5.0f, c.y - 5.0f);
    arrow.lineTo (c.x + 5.0f, c.y + 1.0f);

    g.setColour (colour);
    g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
