#include "KngCompView.h"
#include "KngCompPlugin.h"

namespace te = tracktion;

static const juce::Colour accent (0xff3a8ee6);
static const juce::Colour reductionColour (0xffe6503a);

static juce::String formatComp (int p, double v)
{
    using namespace KngComp;

    switch (p)
    {
        case threshold: case makeup: case knee:
            return juce::String (v, 1) + " dB";

        case ratio:
            return juce::String (v, v < 10.0 ? 1 : 0) + ":1";

        case attack:
            return juce::String (v, v < 1.0 ? 2 : 1) + " ms";

        case release:
            return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms";

        case mix:
            return juce::String (juce::roundToInt (v * 100.0)) + "%";

        case scHpf:
            return juce::String (juce::roundToInt (v)) + " Hz";

        default:
            return juce::String (v, 2);
    }
}

//==============================================================================
KngCompView::KngCompView (KngCompPlugin& owner)
    : plugin (owner), pluginRef (&owner)
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("KNG Comp", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));

    addAndMakeVisible (presetBox);
    presetBox.setTooltip ("Start from a setting made for the job, then adjust");

    {
        juce::StringArray names;

        for (auto& preset : KngComp::presets())
            names.add (preset.name);

        presetBox.attach (plugin, names, [this] (int i) { plugin.applyPreset (i); });
    }

    addAndMakeVisible (modeBox);
    modeBox.setTooltip ("Clean: transparent. Punch: snappy drums. Opto: smooth vocals. Vintage: fast with grit.");

    for (int i = 0; i < KngComp::modeNames().size(); ++i)
        modeBox.addItem (KngComp::modeNames()[i], i + 1);

    modeBox.onChange = [this]
    {
        beginEdit();
        setValue (KngComp::mode, (float) (modeBox.getSelectedId() - 1));
    };

    addAndMakeVisible (autoButton);
    autoButton.setTooltip ("Turns the output back up by about what the compressor takes off");
    autoButton.onClick = [this]
    {
        beginEdit();
        setValue (KngComp::autoMakeup, autoButton.getToggleState() ? 1.0f : 0.0f);
    };

    // One knob per continuous setting
    for (int p : { KngComp::threshold, KngComp::ratio, KngComp::attack, KngComp::release,
                   KngComp::knee, KngComp::makeup, KngComp::mix, KngComp::scHpf })
    {
        const auto& in = KngComp::info (p);
        Knob k;
        k.param = p;

        k.label = std::make_unique<juce::Label> (juce::String(), in.name);
        k.label->setJustificationType (juce::Justification::centred);
        k.label->setFont (juce::Font (juce::FontOptions (11.0f)));
        k.label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
        addAndMakeVisible (*k.label);

        k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        auto* s = k.slider.get();
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
        s->setRange (in.min, in.max, in.step);

        if (in.mid > in.min && in.mid < in.max)
            s->setSkewFactorFromMidPoint (in.mid);

        s->setDoubleClickReturnValue (true, in.def);
        s->textFromValueFunction = [p] (double v) { return formatComp (p, v); };
        s->setValue (value (p), juce::dontSendNotification);
        s->onDragStart   = [this] { beginEdit(); };
        s->onValueChange = [this, p, s] { setValue (p, (float) s->getValue()); };
        addAndMakeVisible (*s);

        knobs.push_back (std::move (k));
    }

    setSize (900, 380);
    startTimerHz (30);
}

KngCompView::~KngCompView()
{
    stopTimer();
}

void KngCompView::beginEdit()
{
    if (auto* um = plugin.getUndoManager())
        um->beginNewTransaction();
}

float KngCompView::value (int p) const
{
    return plugin.getParam (p)->getCurrentValue();
}

void KngCompView::setValue (int p, float v)
{
    plugin.getParam (p)->setParameter (v, juce::sendNotification);
}

//==============================================================================
void KngCompView::timerCallback()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    plugin.takeMeters (in, out, gr);

    const float newIn = juce::Decibels::gainToDecibels (in, -100.0f);
    inDb = newIn > inDb ? newIn : inDb - 1.5f;

    grHistory[(size_t) historyPos] = gr;
    inHistory[(size_t) historyPos] = newIn;
    historyPos = (historyPos + 1) % (int) grHistory.size();

    // Follow automation, presets and undo
    for (auto& k : knobs)
        if (! k.slider->isMouseButtonDown())
            k.slider->setValue (value (k.param), juce::dontSendNotification);

    if (! modeBox.isPopupActive())
        modeBox.setSelectedId (juce::roundToInt (value (KngComp::mode)) + 1, juce::dontSendNotification);

    autoButton.setToggleState (value (KngComp::autoMakeup) > 0.5f, juce::dontSendNotification);

    repaint (transferArea.toNearestInt().getUnion (historyArea.toNearestInt()).expanded (2));
}

//==============================================================================
void KngCompView::drawTransfer (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (area, 6.0f);

    const float T = value (KngComp::threshold), R = value (KngComp::ratio), K = value (KngComp::knee);

    auto toX = [&] (float db) { return area.getX() + (db + 60.0f) / 60.0f * area.getWidth(); };
    auto toY = [&] (float db) { return area.getBottom() - (db + 60.0f) / 60.0f * area.getHeight(); };

    // Grid every 12 dB
    g.setColour (juce::Colours::white.withAlpha (0.06f));

    for (float db = -48.0f; db < 0.0f; db += 12.0f)
    {
        g.drawVerticalLine ((int) toX (db), area.getY(), area.getBottom());
        g.drawHorizontalLine ((int) toY (db), area.getX(), area.getRight());
    }

    // Straight line = no compression
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawLine (area.getX(), area.getBottom(), area.getRight(), area.getY(), 1.0f);

    // Threshold
    g.setColour (accent.withAlpha (0.5f));
    g.drawVerticalLine ((int) toX (T), area.getY(), area.getBottom());

    // The curve: what comes out for what goes in
    juce::Path curve;

    for (float db = -60.0f; db <= 0.0f; db += 0.5f)
    {
        const float y = toY (db + KngComp::gainComputer (db, T, R, K));

        if (curve.isEmpty())
            curve.startNewSubPath (toX (db), y);
        else
            curve.lineTo (toX (db), y);
    }

    g.setColour (juce::Colours::white);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    // Where the signal is right now
    if (inDb > -60.0f)
    {
        const float x = toX (inDb);
        const float y = toY (inDb + KngComp::gainComputer (inDb, T, R, K));
        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ x, y }));
    }

    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("IN", area.reduced (6.0f).toNearestInt(), juce::Justification::bottomRight);
    g.drawText ("OUT", area.reduced (6.0f).toNearestInt(), juce::Justification::topLeft);
}

void KngCompView::drawHistory (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (area, 6.0f);

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (area.toNearestInt());

    const int n = (int) grHistory.size();
    const float colW = area.getWidth() / (float) n;

    for (int k = 0; k < n; ++k)
    {
        const int idx = (historyPos + k) % n;
        const float x = area.getX() + k * colW;

        // Input level, from the bottom
        const float inH = juce::jlimit (0.0f, 1.0f, (inHistory[(size_t) idx] + 60.0f) / 60.0f) * area.getHeight();
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (x, area.getBottom() - inH, colW + 1.0f, inH);

        // Gain reduction, from the top (24 dB = full height)
        const float grH = juce::jlimit (0.0f, 1.0f, grHistory[(size_t) idx] / 24.0f) * area.getHeight();
        g.setColour (reductionColour.withAlpha (0.6f));
        g.fillRect (x, area.getY(), colW + 1.0f, grH);
    }

    // Reduction scale
    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    for (int db : { 6, 12, 18 })
    {
        const float y = area.getY() + db / 24.0f * area.getHeight();
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawText ("-" + juce::String (db), (int) area.getX() + 4, (int) y - 12, 30, 12, juce::Justification::left);
    }

    // Live reduction readout
    const float now = grHistory[(size_t) ((historyPos + n - 1) % n)];
    g.setColour (now > 0.05f ? reductionColour : juce::Colours::white.withAlpha (0.4f));
    g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    g.drawText ((now > 0.05f ? "-" : "") + juce::String (now, 1) + " dB",
                area.reduced (10.0f).toNearestInt(), juce::Justification::topRight);

    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("GAIN REDUCTION", area.reduced (10.0f).toNearestInt().withTrimmedTop (28),
                juce::Justification::topRight);
}

void KngCompView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d20));
    drawTransfer (g, transferArea);
    drawHistory (g, historyArea);
}

void KngCompView::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto top = r.removeFromTop (28);
    titleLabel.setBounds (top.removeFromLeft (110));
    presetBox.setBounds (top.removeFromLeft (200));
    top.removeFromLeft (10);
    modeBox.setBounds (top.removeFromLeft (120));
    top.removeFromLeft (10);
    autoButton.setBounds (top.removeFromLeft (110));

    r.removeFromTop (6);
    auto knobRow = r.removeFromBottom (92);
    r.removeFromBottom (6);

    transferArea = r.removeFromLeft (juce::jmin (r.getHeight(), r.getWidth() / 3)).toFloat();
    r.removeFromLeft (8);
    historyArea = r.toFloat();

    for (auto& k : knobs)
    {
        auto col = knobRow.removeFromLeft (84);
        k.label->setBounds (col.removeFromTop (16));
        k.slider->setBounds (col);
        knobRow.removeFromLeft (4);
    }
}
