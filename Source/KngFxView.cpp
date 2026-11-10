#include "KngFxView.h"

namespace te = tracktion;

static juce::String formatValue (const KngParamDef& d, double v)
{
    const juce::String unit (d.unit);

    if (unit == "%")
        return juce::String (juce::roundToInt (v * 100.0)) + "%";

    if (unit == "Hz")
        return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";

    if (unit == "ms")
        return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " s" : juce::String (v, v < 10.0 ? 1 : 0) + " ms";

    if (unit == "dB")
        return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " dB";

    if (unit == "ct")
        return juce::String (juce::roundToInt (v)) + " ct";

    return juce::String (v, d.step >= 1.0f ? 0 : 2);
}

//==============================================================================
KngFxView::KngFxView (KngFxBase& owner)
    : plugin (owner), pluginRef (&owner)
{
    history.in.assign (300, -100.0f);
    history.out.assign (300, -100.0f);
    history.gr.assign (300, 0.0f);

    addAndMakeVisible (titleLabel);
    titleLabel.setText (plugin.getName(), juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));

    addAndMakeVisible (hintLabel);
    hintLabel.setText (plugin.getHint(), juce::dontSendNotification);
    hintLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    hintLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));

    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Presets");

    for (int i = 0; i < (int) plugin.getPresets().size(); ++i)
        presetBox.addItem (plugin.getPresets()[(size_t) i].name, i + 1);

    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() <= 0)
            return;

        beginEdit();
        plugin.applyPreset (presetBox.getSelectedId() - 1);
    };

    for (int i = 0; i < plugin.getNumParams(); ++i)
    {
        const auto& d = plugin.getDef (i);
        auto* param = plugin.getParam (i);
        const juce::String choices (d.choices != nullptr ? d.choices : "");

        Control c;
        c.param = i;

        if (choices == "toggle")
        {
            c.toggle = std::make_unique<juce::ToggleButton> (d.name);
            auto* t = c.toggle.get();
            t->onClick = [this, param, t]
            {
                beginEdit();
                param->setParameter (t->getToggleState() ? 1.0f : 0.0f, juce::sendNotification);
            };
            addAndMakeVisible (*t);
        }
        else
        {
            c.label = std::make_unique<juce::Label> (juce::String(), d.name);
            c.label->setJustificationType (juce::Justification::centred);
            c.label->setFont (juce::Font (juce::FontOptions (11.0f)));
            c.label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
            addAndMakeVisible (*c.label);

            if (choices.isNotEmpty())
            {
                c.box = std::make_unique<juce::ComboBox>();
                auto* box = c.box.get();
                const auto names = juce::StringArray::fromTokens (choices, "|", "");

                for (int k = 0; k < names.size(); ++k)
                    box->addItem (names[k], k + 1);

                const float minValue = d.min;
                box->onChange = [this, param, box, minValue]
                {
                    beginEdit();
                    param->setParameter (minValue + (float) (box->getSelectedId() - 1), juce::sendNotification);
                };
                addAndMakeVisible (*box);
            }
            else
            {
                c.knob = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
                auto* k = c.knob.get();
                k->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
                k->setRange (d.min, d.max, d.step);

                if (d.mid > d.min && d.mid < d.max)
                    k->setSkewFactorFromMidPoint (d.mid);

                const KngParamDef def = d;
                k->setDoubleClickReturnValue (true, d.def);
                k->textFromValueFunction = [def] (double v) { return formatValue (def, v); };
                k->setValue (param->getCurrentValue(), juce::dontSendNotification);
                k->onDragStart   = [this] { beginEdit(); };
                k->onValueChange = [param, k] { param->setParameter ((float) k->getValue(), juce::sendNotification); };
                addAndMakeVisible (*k);
            }
        }

        controls.push_back (std::move (c));
    }

    setSize (900, 380);
    startTimerHz (30);
}

KngFxView::~KngFxView()
{
    stopTimer();
}

void KngFxView::beginEdit()
{
    if (auto* um = plugin.getUndoManager())
        um->beginNewTransaction();
}

void KngFxView::timerCallback()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    plugin.takeMeters (in, out, gr);

    const float newIn = juce::Decibels::gainToDecibels (in, -100.0f);
    const float newOut = juce::Decibels::gainToDecibels (out, -100.0f);

    history.in[(size_t) history.pos] = newIn;
    history.out[(size_t) history.pos] = newOut;
    history.gr[(size_t) history.pos] = gr;
    history.pos = (history.pos + 1) % (int) history.gr.size();

    inDb  = newIn  > inDb  ? newIn  : inDb  - 1.0f;
    outDb = newOut > outDb ? newOut : outDb - 1.0f;

    // Follow automation, presets and undo
    for (auto& c : controls)
    {
        const float v = plugin.getParam (c.param)->getCurrentValue();

        if (c.knob != nullptr && ! c.knob->isMouseButtonDown())
            c.knob->setValue (v, juce::dontSendNotification);

        if (c.box != nullptr && ! c.box->isPopupActive())
            c.box->setSelectedId (juce::roundToInt (v - plugin.getDef (c.param).min) + 1, juce::dontSendNotification);

        if (c.toggle != nullptr)
            c.toggle->setToggleState (v > 0.5f, juce::dontSendNotification);
    }

    repaint (displayArea.getUnion (inMeterArea).getUnion (outMeterArea).toNearestInt().expanded (2));
}

void KngFxView::drawMeter (juce::Graphics& g, juce::Rectangle<float> area, float db, const juce::String& label)
{
    auto labelArea = area.removeFromBottom (16.0f);

    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (area, 3.0f);

    const float level = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto bar = area.reduced (2.0f);
    g.setColour (db > -0.5f ? juce::Colour (0xffe6503a) : juce::Colour (0xff3a8ee6));
    g.fillRoundedRectangle (bar.removeFromBottom (bar.getHeight() * level), 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (label, labelArea.toNearestInt(), juce::Justification::centred);
}

void KngFxView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d20));
    plugin.drawDisplay (g, displayArea, history);
    drawMeter (g, inMeterArea, inDb, "IN");
    drawMeter (g, outMeterArea, outDb, "OUT");
}

void KngFxView::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto top = r.removeFromTop (28);
    titleLabel.setBounds (top.removeFromLeft (160));
    presetBox.setBounds (top.removeFromLeft (200));
    top.removeFromLeft (12);
    hintLabel.setBounds (top);

    r.removeFromTop (6);
    auto row = r.removeFromBottom (92);
    r.removeFromBottom (6);

    outMeterArea = r.removeFromRight (18).toFloat();
    r.removeFromRight (6);
    inMeterArea = r.removeFromRight (18).toFloat();
    r.removeFromRight (8);
    displayArea = r.toFloat();

    for (auto& c : controls)
    {
        if (c.toggle != nullptr)
        {
            c.toggle->setBounds (row.removeFromLeft (110).withSizeKeepingCentre (104, 26));
        }
        else if (c.box != nullptr)
        {
            auto col = row.removeFromLeft (124);
            c.label->setBounds (col.removeFromTop (16));
            c.box->setBounds (col.withSizeKeepingCentre (116, 26));
        }
        else
        {
            auto col = row.removeFromLeft (84);
            c.label->setBounds (col.removeFromTop (16));
            c.knob->setBounds (col);
        }

        row.removeFromLeft (4);
    }
}
