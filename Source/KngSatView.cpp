#include "KngSatView.h"
#include "KngSatPlugin.h"

namespace te = tracktion;

static const juce::Colour accent (0xff3a8ee6);
static const juce::Colour heat (0xffe6893a);

static juce::String formatSat (int p, double v)
{
    using namespace KngSat;

    switch (p)
    {
        case drive: case output:
            return (v > 0.0 && p == output ? "+" : "") + juce::String (v, 1) + " dB";

        case tone:
            return std::abs (v) < 0.01 ? juce::String ("Neutral")
                 : v > 0.0 ? "Bright " + juce::String (juce::roundToInt (v * 100.0)) + "%"
                           : "Dark " + juce::String (juce::roundToInt (-v * 100.0)) + "%";

        default:
            return juce::String (juce::roundToInt (v * 100.0)) + "%";
    }
}

//==============================================================================
KngSatView::KngSatView (KngSatPlugin& owner)
    : plugin (owner), pluginRef (&owner)
{
    for (auto* c : std::initializer_list<juce::Component*> { &titleLabel, &presetBox, &modeBox, &autoButton })
        addAndMakeVisible (c);

    titleLabel.setText ("KNG Saturator", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));

    {
        juce::StringArray names;

        for (auto& preset : KngSat::presets())
            names.add (preset.name);

        presetBox.attach (plugin, names, [this] (int i) { plugin.applyPreset (i); });
    }

    for (int i = 0; i < KngSat::modeNames().size(); ++i)
        modeBox.addItem (KngSat::modeNames()[i], i + 1);

    modeBox.setTooltip ("Tape: smooth and warm. Tube: body and presence. Soft Clip: thick. Hard Clip: aggressive.");
    modeBox.onChange = [this] { beginEdit(); setValue (KngSat::mode, (float) (modeBox.getSelectedId() - 1)); };

    autoButton.setTooltip ("Keeps the output as loud as the input, so you hear the color, not just more volume");
    autoButton.onClick = [this] { beginEdit(); setValue (KngSat::autoGain, autoButton.getToggleState() ? 1.0f : 0.0f); };

    for (int param : { KngSat::drive, KngSat::tone, KngSat::mix, KngSat::output })
    {
        const auto& in = KngSat::info (param);
        Knob k;
        k.param = param;

        k.label = std::make_unique<juce::Label> (juce::String(), in.name);
        k.label->setJustificationType (juce::Justification::centred);
        k.label->setFont (juce::Font (juce::FontOptions (11.0f)));
        k.label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
        addAndMakeVisible (*k.label);

        k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        auto* s = k.slider.get();
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 16);
        s->setRange (in.min, in.max, in.step);
        s->setDoubleClickReturnValue (true, in.def);
        s->textFromValueFunction = [param] (double v) { return formatSat (param, v); };
        s->setValue (value (param), juce::dontSendNotification);
        s->onDragStart   = [this] { beginEdit(); };
        s->onValueChange = [this, param, s] { setValue (param, (float) s->getValue()); };
        addAndMakeVisible (*s);

        knobs.push_back (std::move (k));
    }

    setSize (760, 380);
    startTimerHz (30);
}

KngSatView::~KngSatView()
{
    stopTimer();
}

void KngSatView::beginEdit()
{
    if (auto* um = plugin.getUndoManager())
        um->beginNewTransaction();
}

float KngSatView::value (int p) const
{
    return plugin.getParam (p)->getCurrentValue();
}

void KngSatView::setValue (int p, float v)
{
    plugin.getParam (p)->setParameter (v, juce::sendNotification);
}

void KngSatView::timerCallback()
{
    float in = 0.0f, out = 0.0f;
    plugin.takeMeters (in, out);

    const float newIn = juce::Decibels::gainToDecibels (in, -100.0f);
    const float newOut = juce::Decibels::gainToDecibels (out, -100.0f);
    inDb  = newIn  > inDb  ? newIn  : inDb  - 1.0f;
    outDb = newOut > outDb ? newOut : outDb - 1.0f;

    for (auto& k : knobs)
        if (! k.slider->isMouseButtonDown())
            k.slider->setValue (value (k.param), juce::dontSendNotification);

    if (! modeBox.isPopupActive())
        modeBox.setSelectedId (juce::roundToInt (value (KngSat::mode)) + 1, juce::dontSendNotification);

    autoButton.setToggleState (value (KngSat::autoGain) > 0.5f, juce::dontSendNotification);

    repaint (curveArea.getUnion (inMeterArea).getUnion (outMeterArea).toNearestInt().expanded (2));
}

//==============================================================================
void KngSatView::drawCurve (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (area, 6.0f);

    const int m = juce::roundToInt (value (KngSat::mode));
    const float driveGain = juce::Decibels::decibelsToGain (value (KngSat::drive));

    auto toX = [&] (float x) { return area.getCentreX() + x * area.getWidth() * 0.45f; };
    auto toY = [&] (float y) { return area.getCentreY() - y * area.getHeight() * 0.45f; };

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
    g.drawVerticalLine ((int) area.getCentreX(), area.getY(), area.getBottom());

    // Clean, for comparison
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawLine (toX (-1.0f), toY (-1.0f), toX (1.0f), toY (1.0f), 1.0f);

    // What this flavor does to the signal at this drive
    juce::Path curve;

    for (float x = -1.0f; x <= 1.0f; x += 0.01f)
    {
        const float y = juce::jlimit (-1.1f, 1.1f, KngSat::shape (m, x * driveGain));

        if (curve.isEmpty())
            curve.startNewSubPath (toX (x), toY (y));
        else
            curve.lineTo (toX (x), toY (y));
    }

    const float heatAmount = value (KngSat::drive) / 24.0f;
    g.setColour (accent.interpolatedWith (heat, heatAmount));
    g.strokePath (curve, juce::PathStrokeType (2.5f));

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    g.drawText (KngSat::modeNames()[m], area.reduced (12.0f).toNearestInt(), juce::Justification::topLeft);
}

void KngSatView::drawMeter (juce::Graphics& g, juce::Rectangle<float> area, float db, const juce::String& label)
{
    auto labelArea = area.removeFromBottom (16.0f);

    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (area, 3.0f);

    const float level = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto bar = area.reduced (2.0f);
    g.setColour (db > -0.5f ? juce::Colour (0xffe6503a) : accent);
    g.fillRoundedRectangle (bar.removeFromBottom (bar.getHeight() * level), 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (label, labelArea.toNearestInt(), juce::Justification::centred);
}

void KngSatView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d20));
    drawCurve (g, curveArea);
    drawMeter (g, inMeterArea, inDb, "IN");
    drawMeter (g, outMeterArea, outDb, "OUT");
}

void KngSatView::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto top = r.removeFromTop (28);
    titleLabel.setBounds (top.removeFromLeft (140));
    presetBox.setBounds (top.removeFromLeft (200));
    top.removeFromLeft (10);
    modeBox.setBounds (top.removeFromLeft (120));
    top.removeFromLeft (10);
    autoButton.setBounds (top.removeFromLeft (110));

    r.removeFromTop (6);
    auto knobRow = r.removeFromBottom (92);
    r.removeFromBottom (6);

    outMeterArea = r.removeFromRight (18).toFloat();
    r.removeFromRight (6);
    inMeterArea = r.removeFromRight (18).toFloat();
    r.removeFromRight (8);
    curveArea = r.toFloat();

    for (auto& k : knobs)
    {
        auto col = knobRow.removeFromLeft (96);
        k.label->setBounds (col.removeFromTop (16));
        k.slider->setBounds (col);
        knobRow.removeFromLeft (4);
    }
}
