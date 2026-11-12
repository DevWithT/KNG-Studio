#include "KngFattenerView.h"
#include "KngPopOut.h"

namespace te = tracktion;

static const juce::Colour red (0xffff3b30), blush (0xffffe9e4), dim (0xff8e5c58), soft (0xffffd0c8), edge (0xff3a1a1c);

static KngFaceStyle makeFatStyle()
{
    KngFaceStyle s;
    s.accent    = red;
    s.bodyLight = juce::Colour (0xff4a2426);
    s.bodyDark  = juce::Colour (0xff1a0d0e);
    s.pointer   = blush;
    s.track     = juce::Colours::white.withAlpha (0.06f);
    s.label     = juce::Colour (0xffc08a85);
    s.value     = juce::Colour (0xffff8a80);
    s.panel     = juce::Colour (0xff2a1213);
    s.outline   = juce::Colour (0xff5a2426);
    s.labelFace = KngFonts::spaceGrotesk;
    s.valueFace = KngFonts::mono;
    s.menuFace  = KngFonts::mono;
    return s;
}

namespace
{
    /** Mono Lows: a pill switch with its name beside it. */
    class PillSwitch : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer
    {
    public:
        PillSwitch (KngFxBase& p, int index, const juce::String& text)
            : plugin (p), pluginRef (&p), param (index), label (text)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            startTimerHz (10);
        }

        ~PillSwitch() override   { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            const bool on = plugin.getParam (param)->getCurrentValue() > 0.5f;
            auto r = getLocalBounds().toFloat();
            const auto pill = r.removeFromLeft (44.0f).withSizeKeepingCentre (44.0f, 24.0f);

            if (on)
            {
                g.setColour (red.withAlpha (0.35f));
                g.fillRoundedRectangle (pill.expanded (3.0f), 15.0f);
            }

            g.setColour (on ? red : edge);
            g.fillRoundedRectangle (pill, 12.0f);

            const float dot = on ? pill.getRight() - 21.0f : pill.getX() + 3.0f;
            g.setColour (juce::Colours::white);
            g.fillEllipse (dot, pill.getY() + 3.0f, 18.0f, 18.0f);

            g.setFont (KngFonts::font (KngFonts::spaceGrotesk, 12.0f, 0.12f));
            g.setColour (blush);
            g.drawText (label.toUpperCase(), r.withTrimmedLeft (10.0f), juce::Justification::centredLeft, false);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            if (auto* um = plugin.getUndoManager())
                um->beginNewTransaction();

            auto* p = plugin.getParam (param);
            p->setParameter (p->getCurrentValue() > 0.5f ? 0.0f : 1.0f, juce::sendNotification);
            repaint();
        }

    private:
        void timerCallback() override   { repaint(); }

        KngFxBase& plugin;
        te::Plugin::Ptr pluginRef;
        const int param;
        juce::String label;
    };
}

//==============================================================================
KngFattenerView::KngFattenerView (KngFattenerPlugin& owner)
    : plugin (owner), pluginRef (&owner), style (makeFatStyle()), fatStyle (makeFatStyle()), look (style)
{
    setLookAndFeel (&look);

    // The FAT knob reads in Anton
    fatStyle.labelFace = KngFonts::anton;
    fatStyle.valueFace = KngFonts::anton;
    fatStyle.value = blush;

    juce::StringArray names;

    for (auto& preset : plugin.getPresets())
        names.add (preset.name);

    presetBox.attach (plugin, names, [this] (int i) { plugin.applyPreset (i); });
    addAndMakeVisible (presetBox);

    addAndMakeVisible (popButton);
    popButton.setTooltip ("Open in its own window");
    popButton.onClick = [this] { KngPopOut::open (plugin); };

    auto make = [this] (int index, const char* label, const KngFaceStyle& s)
    {
        auto k = std::make_unique<KngFaceKnob> (plugin, index, s, label);
        addAndMakeVisible (*k);
        return k;
    };

    fatKnob    = make (KngFattenerPlugin::fat,    "FAT",    fatStyle);
    subKnob    = make (KngFattenerPlugin::sub,    "Sub",    style);
    octaveKnob = make (KngFattenerPlugin::octave, "Octave", style);
    punchKnob  = make (KngFattenerPlugin::punch,  "Punch",  style);
    clipKnob   = make (KngFattenerPlugin::clip,   "Clip",   style);
    driveKnob  = make (KngFattenerPlugin::drive,  "Drive",  style);
    toneKnob   = make (KngFattenerPlugin::tone,   "Tone",   style);
    outputKnob = make (KngFattenerPlugin::output, "Output", style);
    mixKnob    = make (KngFattenerPlugin::mix,    "Mix",    style);

    fatKnob->labelSize = 30.0f;
    fatKnob->valueSize = 20.0f;
    fatKnob->labelColour = red;

    clipKnob->setArcColour (juce::Colour (0xffffb020));

    for (auto* k : { toneKnob.get(), outputKnob.get(), mixKnob.get() })
        k->setArcColour (soft);

    auto sw = std::make_unique<PillSwitch> (plugin, KngFattenerPlugin::monoLows, "Mono lows");
    sw->setTooltip ("Keeps everything under 120 Hz in the center, so the low end hits the same on every speaker.");
    monoSwitch = std::move (sw);
    addAndMakeVisible (*monoSwitch);

    setSize (1000, 540);
    startTimerHz (30);
}

KngFattenerView::~KngFattenerView()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void KngFattenerView::timerCallback()
{
    popButton.setVisible (! KngPopOut::isPoppedOut (*this));

    float in = 0.0f, out = 0.0f, gr = 0.0f;
    plugin.takeMeters (in, out, gr);

    inDb  = juce::jmax (juce::Decibels::gainToDecibels (in, -100.0f), inDb - 1.5f);
    outDb = juce::jmax (juce::Decibels::gainToDecibels (out, -100.0f), outDb - 1.5f);

    repaint (meterArea);
    repaint (curveBox);
}

//==============================================================================
void KngFattenerView::resized()
{
    panel = getLocalBounds().reduced (8);
    auto inner = panel.reduced (26, 18);

    header = inner.removeFromTop (44);
    {
        auto h = header;
        popButton.setBounds (h.removeFromRight (34).withSizeKeepingCentre (34, 34));
        h.removeFromRight (10);
        presetBox.setBounds (h.removeFromRight (200).withSizeKeepingCentre (200, 34));
    }

    inner.removeFromTop (8);
    auto strip = inner.removeFromBottom (juce::jlimit (90, 118, inner.getHeight() / 3));
    inner.removeFromBottom (12);

    // LOW on the left, HIT on the right
    const int sideW = juce::jmin (190, inner.getWidth() / 5);
    auto left = inner.removeFromLeft (sideW);
    auto right = inner.removeFromRight (sideW);
    lowLabel = left.removeFromTop (26);
    hitLabel = right.removeFromTop (26);

    const int cell = left.getHeight() / 2;
    const int s = juce::jlimit (40, 80, cell - 40);

    auto place = [s] (KngFaceKnob& k, juce::Rectangle<int> area)
    {
        k.setBounds (area.withSizeKeepingCentre (s + 40, s + 38));
    };

    place (*subKnob, left.removeFromTop (cell));
    place (*octaveKnob, left);
    place (*punchKnob, right.removeFromTop (cell));
    place (*clipKnob, right);

    // FAT in the middle, with a hint underneath when there's room
    hintArea = inner.getHeight() > 230 ? inner.removeFromBottom (20) : juce::Rectangle<int>();
    const int f = juce::jlimit (80, 220, inner.getHeight() - 64);
    fatKnob->setBounds (inner.withSizeKeepingCentre (f + 40, f + 64));

    // Bottom strip: curve, knobs, then Mono Lows and the meters
    curveBox = strip.removeFromLeft (juce::jmin (230, strip.getWidth() / 4));
    strip.removeFromLeft (18);

    const int ks = juce::jlimit (36, 54, strip.getHeight() - 40);

    for (auto* k : { driveKnob.get(), toneKnob.get(), outputKnob.get(), mixKnob.get() })
        k->setBounds (strip.removeFromLeft (ks + 34).withSizeKeepingCentre (ks + 30, ks + 36));

    strip.removeFromLeft (14);
    stripDividerX = strip.getX();
    strip.removeFromLeft (16);

    auto switchRow = strip.removeFromTop (strip.getHeight() / 2);
    monoSwitch->setBounds (switchRow.withSizeKeepingCentre (switchRow.getWidth(), 30));
    meterArea = strip;
}

void KngFattenerView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0708));

    // Dark red panel, brightest behind the FAT knob
    const auto p = panel.toFloat();
    juce::ColourGradient bg (juce::Colour (0xff3a1416), p.getCentreX(), p.getY() + p.getHeight() * 0.45f,
                             juce::Colour (0xff120708), p.getX(), p.getBottom(), true);
    bg.addColour (0.55, juce::Colour (0xff1c0a0b));
    g.setGradientFill (bg);
    g.fillRoundedRectangle (p, 20.0f);
    g.setColour (edge);
    g.drawRoundedRectangle (p.reduced (0.5f), 20.0f, 1.0f);

    // KNG FATTENER for 808s and kicks
    const auto h = header.toFloat();
    float x = h.getX();

    g.setFont (KngFonts::font (KngFonts::spaceGrotesk, 12.0f, 0.3f));
    g.setColour (dim);
    g.drawText ("KNG", juce::Rectangle<float> (x, h.getY(), 46.0f, h.getHeight()), juce::Justification::centredLeft, false);
    x += 46.0f;

    g.setFont (KngFonts::font (KngFonts::anton, 40.0f, 0.06f));
    g.setColour (blush);
    g.drawText ("FATTENER", juce::Rectangle<float> (x, h.getY(), 230.0f, h.getHeight()), juce::Justification::centredLeft, false);
    x += 226.0f;

    g.setFont (KngFonts::font (KngFonts::spaceGrotesk, 12.0f));
    g.setColour (dim);
    g.drawText ("for 808s and kicks", juce::Rectangle<float> (x, h.getY(), 200.0f, h.getHeight()), juce::Justification::centredLeft, false);

    // LOW and HIT
    g.setFont (KngFonts::font (KngFonts::anton, 18.0f, 0.2f));
    g.setColour (dim);
    g.drawText ("LOW", lowLabel, juce::Justification::centred, false);
    g.drawText ("HIT", hitLabel, juce::Justification::centred, false);

    // A red glow behind FAT
    {
        const auto fb = fatKnob->getBounds().toFloat();
        const float knobH = fb.getHeight() - 58.0f;
        const juce::Point<float> c (fb.getCentreX(), fb.getY() + knobH * 0.5f);
        const float radius = knobH * 0.78f;

        juce::ColourGradient glow (red.withAlpha (0.28f), c.x, c.y, red.withAlpha (0.0f), c.x + radius, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
    }

    if (! hintArea.isEmpty())
    {
        g.setFont (KngFonts::font (KngFonts::spaceGrotesk, 12.0f));
        g.setColour (dim);
        g.drawText ("One knob turns up Sub, Punch and Drive together", hintArea, juce::Justification::centred, false);
    }

    paintCurve (g, curveBox.toFloat());

    g.setColour (edge);
    g.drawVerticalLine (stripDividerX, (float) curveBox.getY(), (float) curveBox.getBottom());

    // IN and OUT
    auto m = meterArea.toFloat().withSizeKeepingCentre ((float) meterArea.getWidth(), 44.0f);
    paintMeter (g, m.removeFromTop (20.0f), "IN", inDb);
    m.removeFromTop (4.0f);
    paintMeter (g, m.removeFromTop (20.0f), "OUT", outDb);
}

//==============================================================================
void KngFattenerView::paintCurve (juce::Graphics& g, juce::Rectangle<float> a)
{
    if (a.getWidth() < 40.0f || a.getHeight() < 40.0f)
        return;

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (a, 12.0f);
    g.setColour (edge);
    g.drawRoundedRectangle (a.reduced (0.5f), 12.0f, 1.0f);

    g.setFont (KngFonts::font (KngFonts::spaceGrotesk, 11.0f, 0.16f));
    g.setColour (style.label);
    g.drawText ("DRIVE CURVE", a.reduced (12.0f, 8.0f).removeFromTop (14.0f), juce::Justification::centredLeft, false);

    const auto plot = a.reduced (12.0f).withTrimmedTop (16.0f);

    // Straight line = no change
    const float dash[] = { 4.0f, 4.0f };
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawDashedLine (juce::Line<float> (plot.getBottomLeft(), plot.getTopRight()), dash, 2, 1.0f);

    juce::Path curve;

    for (int k = 0; k <= 100; ++k)
    {
        const float xIn = (float) k / 100.0f;
        const float yOut = juce::jlimit (0.0f, 1.0f, plugin.curveAt (xIn));
        const float px = plot.getX() + xIn * plot.getWidth();
        const float py = plot.getBottom() - yOut * plot.getHeight();

        if (k == 0)
            curve.startNewSubPath (px, py);
        else
            curve.lineTo (px, py);
    }

    g.setColour (red.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (red);
    g.strokePath (curve, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void KngFattenerView::paintMeter (juce::Graphics& g, juce::Rectangle<float> row, const juce::String& label, float db)
{
    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    g.setColour (dim);
    g.drawText (label, row.removeFromLeft (36.0f), juce::Justification::centredLeft, false);

    g.setColour (blush);
    g.drawText (db > -60.0f ? juce::String (db, 1) : juce::String ("-inf"), row.removeFromRight (48.0f),
                juce::Justification::centredRight, false);

    const auto bar = row.withSizeKeepingCentre (row.getWidth() - 8.0f, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (bar, 4.0f);

    const float level = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    g.setColour (db > -0.5f ? juce::Colour (0xffffb020) : red);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * level), 4.0f);
}
