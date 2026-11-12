#include "Kng808View.h"
#include "KngPopOut.h"

namespace te = tracktion;

static const juce::Colour orange (0xffff7a1a), cream (0xfff4ede4), dimText (0xff7d766e);

static KngFaceStyle make808Style()
{
    KngFaceStyle s;
    s.accent    = orange;
    s.bodyLight = juce::Colour (0xff3b3d41);
    s.bodyDark  = juce::Colour (0xff151619);
    s.pointer   = cream;
    s.track     = juce::Colours::white.withAlpha (0.07f);
    s.label     = juce::Colour (0xffa59c92);
    s.value     = juce::Colour (0xffff9a4a);
    s.panel     = juce::Colour (0xff0c0d0e);
    s.outline   = juce::Colour (0xff2e2f33);
    s.labelFace = KngFonts::barlowSemi;
    s.valueFace = KngFonts::mono;
    s.menuFace  = KngFonts::mono;
    return s;
}

static void drawScrew (juce::Graphics& g, juce::Point<float> c)
{
    juce::ColourGradient shade (juce::Colour (0xff55585d), c.x - 2.0f, c.y - 2.0f,
                                juce::Colour (0xff1d1e21), c.x + 5.0f, c.y + 5.0f, true);
    g.setGradientFill (shade);
    g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
    g.setColour (juce::Colour (0xff0d0d0e));
    g.drawLine (c.x - 3.0f, c.y - 2.0f, c.x + 3.0f, c.y + 2.0f, 1.5f);
}

//==============================================================================
Kng808View::Kng808View (Kng808Plugin& owner)
    : plugin (owner), pluginRef (&owner), style (make808Style()), look (style)
{
    setLookAndFeel (&look);

    juce::StringArray names;

    for (auto& preset : plugin.getPresets())
        names.add (preset.name);

    presetBox.attach (plugin, names, [this] (int i) { plugin.applyPreset (i); });
    addAndMakeVisible (presetBox);

    addAndMakeVisible (popButton);
    popButton.setTooltip ("Open in its own window");
    popButton.onClick = [this] { KngPopOut::open (plugin); };

    auto make = [this] (int index, const char* label)
    {
        auto k = std::make_unique<KngFaceKnob> (plugin, index, style, label);
        addAndMakeVisible (*k);
        return k;
    };

    lengthKnob   = make (Kng808Plugin::length,   "Length");
    driveKnob    = make (Kng808Plugin::drive,    "Drive");
    tuneKnob     = make (Kng808Plugin::tune,     "Tune");
    dropKnob     = make (Kng808Plugin::drop,     "Pitch Drop");
    dropTimeKnob = make (Kng808Plugin::dropTime, "Drop Time");
    punchKnob    = make (Kng808Plugin::punch,    "Punch");
    glideKnob    = make (Kng808Plugin::glide,    "Glide");
    levelKnob    = make (Kng808Plugin::level,    "Level");

    // The two big ones
    for (auto* k : { lengthKnob.get(), driveKnob.get() })
    {
        k->labelSize = 16.0f;
        k->valueSize = 13.0f;
        k->labelColour = cream;
    }

    driveKnob->setArcColour (juce::Colour (0xffff4d1a));
    levelKnob->setArcColour (cream);

    holdSwitch = std::make_unique<KngFaceToggle> (plugin, Kng808Plugin::hold, style, "Hold Notes");
    holdSwitch->setTooltip ("On: the 808 stops when the note ends. Off: every hit rings its full Length.");
    addAndMakeVisible (*holdSwitch);

    setSize (1000, 520);
    startTimerHz (20);
}

Kng808View::~Kng808View()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Kng808View::timerCallback()
{
    // The arrow is only needed while docked
    popButton.setVisible (! KngPopOut::isPoppedOut (*this));

    const int note = plugin.displayNote.load();

    if (note != shownNote)
    {
        shownNote = note;
        repaint (keysArea.expanded (4, 24));
    }

    repaint (screen);
}

//==============================================================================
void Kng808View::resized()
{
    panel = getLocalBounds().reduced (8);
    auto inner = panel.reduced (24, 16);

    header = inner.removeFromTop (44);
    {
        auto h = header;
        popButton.setBounds (h.removeFromRight (34).withSizeKeepingCentre (34, 34));
        h.removeFromRight (10);
        presetBox.setBounds (h.removeFromRight (220).withSizeKeepingCentre (220, 34));
    }

    inner.removeFromTop (10);

    // Bottom row: small knobs, the Hold switch and the keys
    const int rowH = juce::jlimit (96, 120, inner.getHeight() / 3);
    auto bottom = inner.removeFromBottom (rowH);
    inner.removeFromBottom (12);
    dividerY = bottom.getY() - 6;

    keysArea = bottom.removeFromRight (130).withSizeKeepingCentre (112, 50).translated (0, -8);
    holdSwitch->setBounds (bottom.removeFromRight (120).withSizeKeepingCentre (110, 64));

    const int small = juce::jlimit (40, 62, bottom.getHeight() - 38);

    for (auto* k : { tuneKnob.get(), dropKnob.get(), dropTimeKnob.get(), punchKnob.get(), glideKnob.get(), levelKnob.get() })
    {
        auto col = bottom.removeFromLeft (small + 46);
        k->setBounds (col.withSizeKeepingCentre (small + 40, small + 36));
    }

    // Big knobs to the right of the screen
    const int big = juce::jlimit (64, 130, inner.getHeight() - 52);
    auto bigArea = inner.removeFromRight (big * 2 + 70);
    screen = inner.withTrimmedRight (20);

    auto row = bigArea.withSizeKeepingCentre (bigArea.getWidth(), big + 44);
    lengthKnob->setBounds (row.removeFromLeft (row.getWidth() / 2).withSizeKeepingCentre (big + 30, big + 44));
    driveKnob->setBounds (row.withSizeKeepingCentre (big + 30, big + 44));
}

void Kng808View::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0c0d));

    // The hardware panel
    const auto p = panel.toFloat();
    juce::ColourGradient body (juce::Colour (0xff1d1e21), 0.0f, p.getY(), juce::Colour (0xff131416), 0.0f, p.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (p, 14.0f);
    g.setColour (juce::Colour (0xff2b2c30));
    g.drawRoundedRectangle (p.reduced (0.5f), 14.0f, 1.0f);

    drawScrew (g, { p.getX() + 12.0f, p.getY() + 12.0f });
    drawScrew (g, { p.getRight() - 12.0f, p.getY() + 12.0f });
    drawScrew (g, { p.getX() + 12.0f, p.getBottom() - 12.0f });
    drawScrew (g, { p.getRight() - 12.0f, p.getBottom() - 12.0f });

    // KNG 808 BASS MACHINE, with a glow on the 808
    const auto h = header.toFloat();
    float x = h.getX();

    g.setFont (KngFonts::font (KngFonts::barlowSemi, 14.0f, 0.3f));
    g.setColour (dimText);
    g.drawText ("KNG", juce::Rectangle<float> (x, h.getY(), 46.0f, h.getHeight()), juce::Justification::centredLeft, false);
    x += 48.0f;

    g.setFont (KngFonts::font (KngFonts::barlowHeavy, 46.0f));
    const juce::Rectangle<float> logo (x, h.getY(), 96.0f, h.getHeight());
    g.setColour (orange.withAlpha (0.16f));

    for (auto offset : { juce::Point<float> (-2.0f, 0.0f), { 2.0f, 0.0f }, { 0.0f, -2.0f }, { 0.0f, 2.0f } })
        g.drawText ("808", logo.translated (offset.x, offset.y), juce::Justification::centredLeft, false);

    g.setColour (orange);
    g.drawText ("808", logo, juce::Justification::centredLeft, false);
    x += 92.0f;

    g.setFont (KngFonts::font (KngFonts::barlowSemi, 14.0f, 0.2f));
    g.setColour (dimText);
    g.drawText ("BASS MACHINE", juce::Rectangle<float> (x, h.getY(), 160.0f, h.getHeight()), juce::Justification::centredLeft, false);

    // Line above the bottom row
    g.setColour (juce::Colour (0xff26272a));
    g.drawHorizontalLine (dividerY, (float) screen.getX(), (float) panel.getRight() - 24.0f);

    paintScreen (g, screen.toFloat());
    paintKeys (g, keysArea.toFloat());
}

//==============================================================================
void Kng808View::paintScreen (juce::Graphics& g, juce::Rectangle<float> a)
{
    if (a.getWidth() < 40.0f || a.getHeight() < 40.0f)
        return;

    g.setColour (juce::Colour (0xff090a0a));
    g.fillRoundedRectangle (a, 10.0f);
    g.setColour (juce::Colour (0xff26272a));
    g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);

    {
        juce::Graphics::ScopedSaveState saveState (g);
        g.reduceClipRegion (a.reduced (1.0f).toNearestInt());
        g.setColour (juce::Colours::white.withAlpha (0.035f));

        for (float gx = a.getX() + 40.0f; gx < a.getRight(); gx += 40.0f)
            g.drawVerticalLine ((int) gx, a.getY(), a.getBottom());

        for (float gy = a.getY() + 30.0f; gy < a.getBottom(); gy += 30.0f)
            g.drawHorizontalLine ((int) gy, a.getX(), a.getRight());
    }

    const auto plot = a.reduced (14.0f).withTrimmedTop (22.0f).withTrimmedBottom (16.0f);
    const float lengthS = juce::jmax (0.01f, plugin.getParam (Kng808Plugin::length)->getCurrentValue() * 0.001f);
    const float dropSt = plugin.getParam (Kng808Plugin::drop)->getCurrentValue();
    const float tau = juce::jmax (0.0005f, plugin.getParam (Kng808Plugin::dropTime)->getCurrentValue() * 0.001f / 3.0f);
    const float span = juce::jmax (0.3f, lengthS * 1.1f);

    juce::Path volume, pitch;

    for (int k = 0; k <= 160; ++k)
    {
        const float t = span * (float) k / 160.0f;
        const float px = plot.getX() + plot.getWidth() * (float) k / 160.0f;
        const float amp = std::exp (-6.9f * t / lengthS);
        const float drop01 = juce::jlimit (0.0f, 1.0f, dropSt * std::exp (-t / tau) / 24.0f);

        const float vy = plot.getBottom() - amp * plot.getHeight();
        const float py = plot.getBottom() - (0.06f + drop01 * 0.88f) * plot.getHeight();

        if (k == 0)
        {
            volume.startNewSubPath (px, vy);
            pitch.startNewSubPath (px, py);
        }
        else
        {
            volume.lineTo (px, vy);
            pitch.lineTo (px, py);
        }
    }

    // Volume: a soft fill with a cream line
    auto fill = volume;
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();

    juce::ColourGradient fade (cream.withAlpha (0.22f), 0.0f, plot.getY(), cream.withAlpha (0.0f), 0.0f, plot.getBottom(), false);
    g.setGradientFill (fade);
    g.fillPath (fill);
    g.setColour (cream.withAlpha (0.75f));
    g.strokePath (volume, juce::PathStrokeType (2.0f));

    // Pitch: glowing orange
    g.setColour (orange.withAlpha (0.25f));
    g.strokePath (pitch, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (orange);
    g.strokePath (pitch, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Legend
    g.setFont (KngFonts::font (KngFonts::mono, 10.0f, 0.08f));
    g.setColour (juce::Colour (0xff6e675f));
    g.drawText ("PITCH", juce::Rectangle<float> (a.getX() + 16.0f, a.getY() + 8.0f, 44.0f, 14.0f), juce::Justification::centredLeft, false);
    g.setColour (orange);
    g.fillRect (a.getX() + 56.0f, a.getY() + 12.0f, 7.0f, 7.0f);
    g.setColour (juce::Colour (0xff6e675f));
    g.drawText ("VOLUME", juce::Rectangle<float> (a.getX() + 74.0f, a.getY() + 8.0f, 54.0f, 14.0f), juce::Justification::centredLeft, false);
    g.setColour (cream);
    g.fillRect (a.getX() + 122.0f, a.getY() + 12.0f, 7.0f, 7.0f);

    // The note playing now
    const int note = plugin.displayNote.load();
    const auto readout = juce::Rectangle<float> (a.getRight() - 150.0f, a.getY() + 6.0f, 136.0f, 30.0f);

    g.setFont (KngFonts::font (KngFonts::mono, 26.0f));

    if (note >= 0)
    {
        const float tune = plugin.getParam (Kng808Plugin::tune)->getCurrentValue();
        const int shown = note + juce::roundToInt (tune);
        const double hz = 440.0 * std::exp2 ((note + tune - 69.0) / 12.0);

        g.setColour (juce::Colour (0xffff9a4a));
        g.drawText (juce::MidiMessage::getMidiNoteName (shown, true, true, 3), readout, juce::Justification::centredRight, false);
        g.setFont (KngFonts::font (KngFonts::mono, 11.0f));
        g.setColour (juce::Colour (0xff6e675f));
        g.drawText (juce::String (hz, 1) + " Hz", readout.translated (0.0f, 26.0f).withHeight (14.0f), juce::Justification::centredRight, false);
    }
    else
    {
        g.setColour (juce::Colour (0xff3a3632));
        g.drawText ("--", readout, juce::Justification::centredRight, false);
    }

    // Time along the bottom
    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
    g.setColour (juce::Colour (0xff5b554f));

    for (int i = 0; i <= 4; ++i)
    {
        const float t = span * (float) i / 4.0f;
        const float tx = plot.getX() + plot.getWidth() * (float) i / 4.0f;
        const auto text = i == 0 ? juce::String ("0") : juce::String (t, 2) + " s";
        const auto just = i == 0 ? juce::Justification::centredLeft : i == 4 ? juce::Justification::centredRight : juce::Justification::centred;
        const float w = 60.0f;
        const float left = i == 0 ? tx : i == 4 ? tx - w : tx - w * 0.5f;

        g.drawText (text, juce::Rectangle<float> (left, a.getBottom() - 16.0f, w, 12.0f), just, false);
    }
}

void Kng808View::paintKeys (juce::Graphics& g, juce::Rectangle<float> a)
{
    if (a.isEmpty())
        return;

    const int note = plugin.displayNote.load();
    const int pc = note >= 0 ? note % 12 : -1;

    // One octave: the playing note lights up whatever octave it's in
    static const int whitePcs[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int blackPcs[5] = { 1, 3, 6, 8, 10 };
    static const int blackAfter[5] = { 0, 1, 3, 4, 5 };

    const float kw = a.getWidth() / 7.0f;

    for (int i = 0; i < 7; ++i)
    {
        const auto key = juce::Rectangle<float> (a.getX() + i * kw + 1.0f, a.getY(), kw - 2.0f, a.getHeight());
        const bool lit = whitePcs[i] == pc;

        if (lit)
        {
            g.setColour (orange.withAlpha (0.35f));
            g.fillRoundedRectangle (key.expanded (3.0f), 4.0f);
        }

        g.setColour (lit ? orange : juce::Colour (0xffe9e3da));
        g.fillRoundedRectangle (key, 3.0f);
    }

    for (int i = 0; i < 5; ++i)
    {
        const float cx = a.getX() + (blackAfter[i] + 1) * kw;
        const auto key = juce::Rectangle<float> (cx - kw * 0.32f, a.getY(), kw * 0.64f, a.getHeight() * 0.6f);
        g.setColour (blackPcs[i] == pc ? orange : juce::Colour (0xff1d1e21));
        g.fillRoundedRectangle (key, 2.0f);
    }

    g.setFont (KngFonts::font (KngFonts::barlowSemi, 13.0f, 0.12f));
    g.setColour (juce::Colour (0xffa59c92));
    g.drawText ("PLAYING", juce::Rectangle<float> (a.getX() - 10.0f, a.getBottom() + 4.0f, a.getWidth() + 20.0f, 16.0f),
                juce::Justification::centred, false);
}
