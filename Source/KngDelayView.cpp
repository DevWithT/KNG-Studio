#include "KngDelayView.h"
#include "KngDelayPlugin.h"

namespace te = tracktion;

static const juce::Colour accent (0xff3a8ee6);
static const juce::Colour rightColour (0xffe6a23a);

static juce::String formatDelay (int p, double v)
{
    using namespace KngDelay;

    switch (p)
    {
        case timeL: case timeR:
            return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms";

        case lowCut: case highCut:
            return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";

        default:
            return juce::String (juce::roundToInt (v * 100.0)) + "%";
    }
}

//==============================================================================
KngDelayView::KngDelayView (KngDelayPlugin& owner)
    : plugin (owner), pluginRef (&owner)
{
    for (auto* c : std::initializer_list<juce::Component*> { &titleLabel, &presetBox, &modeBox, &syncButton,
                                                             &divLBox, &divRBox, &leftLabel, &rightLabel })
        addAndMakeVisible (c);

    titleLabel.setText ("KNG Delay", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));

    presetBox.setTooltip ("Start from a setting made for the job, then adjust. Use Send Bus when it's on a bus.");

    {
        juce::StringArray names;

        for (auto& preset : KngDelay::presets())
            names.add (preset.name);

        presetBox.attach (plugin, names, [this] (int i) { plugin.applyPreset (i); });
    }

    for (int i = 0; i < KngDelay::modeNames().size(); ++i)
        modeBox.addItem (KngDelay::modeNames()[i], i + 1);

    modeBox.setTooltip ("Stereo: separate sides. Ping-Pong: bounces left to right. Mono: one echo, center.");
    modeBox.onChange = [this] { beginEdit(); setValue (KngDelay::mode, (float) (modeBox.getSelectedId() - 1)); };

    syncButton.setTooltip ("Lock the echoes to the song's tempo");
    syncButton.onClick = [this] { beginEdit(); setValue (KngDelay::sync, syncButton.getToggleState() ? 1.0f : 0.0f); };

    for (auto* box : { &divLBox, &divRBox })
        for (int i = 0; i < KngDelay::divisionNames().size(); ++i)
            box->addItem (KngDelay::divisionNames()[i], i + 1);

    divLBox.setTooltip ("Note length for the left echo (T = triplet, D = dotted)");
    divRBox.setTooltip ("Note length for the right echo (T = triplet, D = dotted)");
    divLBox.onChange = [this] { beginEdit(); setValue (KngDelay::divL, (float) (divLBox.getSelectedId() - 1)); };
    divRBox.onChange = [this] { beginEdit(); setValue (KngDelay::divR, (float) (divRBox.getSelectedId() - 1)); };

    for (auto* l : { &leftLabel, &rightLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::Font (juce::FontOptions (11.0f)));
        l->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    }

    timeLKnob = makeKnob (KngDelay::timeL);
    timeRKnob = makeKnob (KngDelay::timeR);

    for (int param : { KngDelay::feedback, KngDelay::lowCut, KngDelay::highCut, KngDelay::drive,
                       KngDelay::wobble, KngDelay::width, KngDelay::duck, KngDelay::mix })
        knobs.push_back (makeKnob (param));

    setSize (980, 380);
    startTimerHz (30);
}

KngDelayView::~KngDelayView()
{
    stopTimer();
}

KngDelayView::Knob KngDelayView::makeKnob (int param)
{
    const auto& in = KngDelay::info (param);
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
    s->textFromValueFunction = [param] (double v) { return formatDelay (param, v); };
    s->setValue (value (param), juce::dontSendNotification);
    s->onDragStart   = [this] { beginEdit(); };
    s->onValueChange = [this, param, s] { setValue (param, (float) s->getValue()); };
    addAndMakeVisible (*s);

    return k;
}

void KngDelayView::beginEdit()
{
    if (auto* um = plugin.getUndoManager())
        um->beginNewTransaction();
}

float KngDelayView::value (int p) const
{
    return plugin.getParam (p)->getCurrentValue();
}

void KngDelayView::setValue (int p, float v)
{
    plugin.getParam (p)->setParameter (v, juce::sendNotification);
}

//==============================================================================
void KngDelayView::timerCallback()
{
    const float db = juce::Decibels::gainToDecibels (plugin.takeWetPeak(), -100.0f);
    wetDb = db > wetDb ? db : wetDb - 1.0f;

    const bool synced = value (KngDelay::sync) > 0.5f;
    const bool isMono = juce::roundToInt (value (KngDelay::mode)) == KngDelay::mono;

    // Synced: note-length boxes. Free: millisecond knobs.
    divLBox.setVisible (synced);
    divRBox.setVisible (synced && ! isMono);
    timeLKnob.slider->setVisible (! synced);
    timeRKnob.slider->setVisible (! synced && ! isMono);
    timeLKnob.label->setVisible (false);
    timeRKnob.label->setVisible (false);
    rightLabel.setVisible (! isMono);

    syncButton.setToggleState (synced, juce::dontSendNotification);

    if (! modeBox.isPopupActive())
        modeBox.setSelectedId (juce::roundToInt (value (KngDelay::mode)) + 1, juce::dontSendNotification);

    if (! divLBox.isPopupActive())
        divLBox.setSelectedId (juce::roundToInt (value (KngDelay::divL)) + 1, juce::dontSendNotification);

    if (! divRBox.isPopupActive())
        divRBox.setSelectedId (juce::roundToInt (value (KngDelay::divR)) + 1, juce::dontSendNotification);

    for (auto* k : { &timeLKnob, &timeRKnob })
        if (! k->slider->isMouseButtonDown())
            k->slider->setValue (value (k->param), juce::dontSendNotification);

    for (auto& k : knobs)
        if (! k.slider->isMouseButtonDown())
            k.slider->setValue (value (k.param), juce::dontSendNotification);

    repaint (echoArea.getUnion (meterArea).toNearestInt().expanded (2));
}

//==============================================================================
void KngDelayView::drawEchoes (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (area, 6.0f);

    const int m = juce::roundToInt (value (KngDelay::mode));
    const double dL = plugin.getDelaySeconds (0);
    const double dR = m == KngDelay::mono ? dL : plugin.getDelaySeconds (1);
    const float fb = value (KngDelay::feedback);
    const double window = juce::jlimit (1.0, 6.0, juce::jmax (dL, dR) * 7.0);

    auto toX = [&] (double t) { return area.getX() + 12.0f + (float) (t / window) * (area.getWidth() - 24.0f); };
    const float midY = area.getCentreY();
    const float half = area.getHeight() * 0.5f - 22.0f;

    // Centre line and the dry hit at 0
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine ((int) midY, area.getX(), area.getRight());
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.fillRect (toX (0.0) - 1.0f, midY - half, 3.0f, half * 2.0f);

    // Each echo, shrinking as it repeats (left on top, right underneath)
    auto drawTap = [&] (double t, float amp, bool isLeft)
    {
        if (t > window || amp < 0.01f)
            return;

        const float h = half * amp;
        g.setColour ((isLeft ? accent : rightColour).withAlpha (0.4f + 0.6f * amp));
        g.fillRoundedRectangle (toX (t) - 2.0f, isLeft ? midY - h : midY, 4.0f, h, 2.0f);
    };

    for (int k = 1; k <= 24; ++k)
    {
        const float amp = std::pow (fb, (float) (k - 1));

        if (m == KngDelay::pingPong)
        {
            // Left first, then it bounces right, then left...
            const int lefts = (k + 1) / 2, rights = k / 2;
            drawTap (lefts * dL + rights * dR, amp, k % 2 == 1);
        }
        else
        {
            drawTap (k * dL, amp, true);
            drawTap (k * dR, amp, false);
        }
    }

    // Readout
    auto describe = [this] (int side, double seconds)
    {
        const auto ms = juce::String (juce::roundToInt (seconds * 1000.0)) + " ms";

        if (value (KngDelay::sync) < 0.5f)
            return ms;

        const int div = juce::roundToInt (value (side == 0 ? KngDelay::divL : KngDelay::divR));
        return KngDelay::divisionNames()[div] + " = " + ms;
    };

    juce::String text = "L " + describe (0, dL);

    if (m != KngDelay::mono)
        text << "     R " + describe (1, dR);

    text << "     @ " << juce::String (plugin.getBpm(), 1) << " BPM";

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (text, area.reduced (12.0f, 6.0f).toNearestInt(), juce::Justification::topLeft);
}

void KngDelayView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d20));
    drawEchoes (g, echoArea);

    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (meterArea, 3.0f);

    const float level = juce::jlimit (0.0f, 1.0f, (wetDb + 60.0f) / 60.0f);
    auto bar = meterArea.reduced (2.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (bar.removeFromBottom (bar.getHeight() * level), 2.0f);
}

void KngDelayView::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto top = r.removeFromTop (28);
    titleLabel.setBounds (top.removeFromLeft (110));
    presetBox.setBounds (top.removeFromLeft (200));
    top.removeFromLeft (10);
    modeBox.setBounds (top.removeFromLeft (120));
    top.removeFromLeft (10);
    syncButton.setBounds (top.removeFromLeft (140));

    r.removeFromTop (6);
    auto knobRow = r.removeFromBottom (92);
    r.removeFromBottom (6);

    meterArea = r.removeFromRight (14).toFloat();
    r.removeFromRight (8);
    echoArea = r.toFloat();

    // Left and right: a note-length box when synced, a time knob when free
    auto timeCell = [&knobRow] (juce::Label& label, juce::ComboBox& box, Knob& knob)
    {
        auto col = knobRow.removeFromLeft (96);
        label.setBounds (col.removeFromTop (16));
        box.setBounds (col.withSizeKeepingCentre (88, 26));
        knob.slider->setBounds (col);
        knobRow.removeFromLeft (4);
    };

    timeCell (leftLabel, divLBox, timeLKnob);
    timeCell (rightLabel, divRBox, timeRKnob);
    knobRow.removeFromLeft (8);

    for (auto& k : knobs)
    {
        auto col = knobRow.removeFromLeft (84);
        k.label->setBounds (col.removeFromTop (16));
        k.slider->setBounds (col);
        knobRow.removeFromLeft (4);
    }
}
