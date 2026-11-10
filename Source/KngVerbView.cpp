#include "KngVerbView.h"
#include "KngVerbPlugin.h"

namespace te = tracktion;

static const juce::Colour accent (0xff3a8ee6);

static juce::String formatVerb (int p, double v)
{
    using namespace KngVerb;

    switch (p)
    {
        case preDelay:
            return juce::String (juce::roundToInt (v)) + " ms";

        case decay:
            return juce::String (v, v < 10.0 ? 2 : 1) + " s";

        case damping: case lowCut: case highCut:
            return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";

        default:
            return juce::String (juce::roundToInt (v * 100.0)) + "%";
    }
}

//==============================================================================
KngVerbView::KngVerbView (KngVerbPlugin& owner)
    : plugin (owner), pluginRef (&owner)
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("KNG Verb", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));

    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setTooltip ("Start from a space made for the job, then adjust. Use Send Bus when it's on a bus.");

    const auto& list = KngVerb::presets();

    for (int i = 0; i < (int) list.size(); ++i)
        presetBox.addItem (list[(size_t) i].name, i + 1);

    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() <= 0)
            return;

        beginEdit();
        plugin.applyPreset (presetBox.getSelectedId() - 1);
    };

    for (int param = 0; param < KngVerb::numParams; ++param)
    {
        const auto& in = KngVerb::info (param);
        Knob k;
        k.param = param;

        k.label = std::make_unique<juce::Label> (juce::String(), in.name);
        k.label->setJustificationType (juce::Justification::centred);
        k.label->setFont (juce::Font (juce::FontOptions (11.0f)));
        k.label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
        addAndMakeVisible (*k.label);

        k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        auto* s = k.slider.get();
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        s->setRange (in.min, in.max, in.step);

        if (in.mid > in.min && in.mid < in.max)
            s->setSkewFactorFromMidPoint (in.mid);

        s->setDoubleClickReturnValue (true, in.def);
        s->textFromValueFunction = [param] (double v) { return formatVerb (param, v); };
        s->setValue (value (param), juce::dontSendNotification);
        s->onDragStart   = [this] { beginEdit(); };
        s->onValueChange = [this, param, s] { setValue (param, (float) s->getValue()); };
        addAndMakeVisible (*s);

        knobs.push_back (std::move (k));
    }

    setSize (980, 380);
    startTimerHz (30);
}

KngVerbView::~KngVerbView()
{
    stopTimer();
}

void KngVerbView::beginEdit()
{
    if (auto* um = plugin.getUndoManager())
        um->beginNewTransaction();
}

float KngVerbView::value (int p) const
{
    return plugin.getParam (p)->getCurrentValue();
}

void KngVerbView::setValue (int p, float v)
{
    plugin.getParam (p)->setParameter (v, juce::sendNotification);
}

void KngVerbView::timerCallback()
{
    const float db = juce::Decibels::gainToDecibels (plugin.takeWetPeak(), -100.0f);
    wetDb = db > wetDb ? db : wetDb - 1.0f;

    for (auto& k : knobs)
        if (! k.slider->isMouseButtonDown())
            k.slider->setValue (value (k.param), juce::dontSendNotification);

    repaint (tailArea.getUnion (bandArea).getUnion (meterArea).toNearestInt().expanded (2));
}

//==============================================================================
void KngVerbView::drawTail (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (area, 6.0f);

    const float pre = value (KngVerb::preDelay) * 0.001f;
    const float decaySec = value (KngVerb::decay);
    const float total = juce::jmax (1.0f, pre + decaySec * 1.3f);

    auto toX = [&] (float t)  { return area.getX() + t / total * area.getWidth(); };
    auto toY = [&] (float db) { return area.getY() + 10.0f + (-db / 60.0f) * (area.getHeight() - 20.0f); };

    // Time grid
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    const float step = total > 8.0f ? 2.0f : total > 3.0f ? 1.0f : 0.5f;

    for (float t = step; t < total; t += step)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) toX (t), area.getY(), area.getBottom());
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawText (juce::String (t, step < 1.0f ? 1 : 0) + "s", (int) toX (t) + 3, (int) area.getBottom() - 14, 40, 12,
                    juce::Justification::left);
    }

    // The tail: silence for the pre-delay, then a fade to -60 dB over the decay time
    const float darkness = 1.0f - (value (KngVerb::damping) - 1000.0f) / 19000.0f;
    const auto tailColour = accent.interpolatedWith (juce::Colour (0xff6a3ae6), darkness * 0.8f);

    juce::Path tail;
    tail.startNewSubPath (toX (pre), area.getBottom());

    for (float t = pre; t <= total; t += total / 200.0f)
    {
        const float db = -60.0f * (t - pre) / juce::jmax (0.05f, decaySec);
        tail.lineTo (toX (t), toY (juce::jmax (-60.0f, db)));
    }

    tail.lineTo (toX (total), area.getBottom());
    tail.closeSubPath();

    g.setColour (tailColour.withAlpha (0.35f));
    g.fillPath (tail);
    g.setColour (tailColour);
    g.strokePath (tail, juce::PathStrokeType (1.5f));

    // Pre-delay marker
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.drawVerticalLine ((int) toX (pre), area.getY(), area.getBottom());

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    g.drawText (formatVerb (KngVerb::decay, decaySec), area.reduced (12.0f).toNearestInt(), juce::Justification::topRight);

    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("DECAY", area.reduced (12.0f).toNearestInt().withTrimmedTop (26), juce::Justification::topRight);
}

void KngVerbView::drawBand (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (area, 4.0f);

    auto toX = [&] (float f) { return area.getX() + area.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };

    const float x1 = toX (value (KngVerb::lowCut));
    const float x2 = toX (value (KngVerb::highCut));

    g.setColour (accent.withAlpha (0.4f));
    g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (x1, area.getY() + 3.0f, x2, area.getBottom() - 3.0f), 3.0f);

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("REVERB RANGE", area.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
}

void KngVerbView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d20));
    drawTail (g, tailArea);
    drawBand (g, bandArea);

    // Reverb level meter
    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (meterArea, 3.0f);

    const float level = juce::jlimit (0.0f, 1.0f, (wetDb + 60.0f) / 60.0f);
    auto bar = meterArea.reduced (2.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (bar.removeFromBottom (bar.getHeight() * level), 2.0f);
}

void KngVerbView::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto top = r.removeFromTop (28);
    titleLabel.setBounds (top.removeFromLeft (110));
    presetBox.setBounds (top.removeFromLeft (200));

    r.removeFromTop (6);
    auto knobRow = r.removeFromBottom (92);
    r.removeFromBottom (6);

    meterArea = r.removeFromRight (14).toFloat();
    r.removeFromRight (8);
    bandArea = r.removeFromBottom (22).toFloat();
    r.removeFromBottom (6);
    tailArea = r.toFloat();

    for (auto& k : knobs)
    {
        auto col = knobRow.removeFromLeft (84);
        k.label->setBounds (col.removeFromTop (16));
        k.slider->setBounds (col);
        knobRow.removeFromLeft (4);
    }
}
