#include "KngSynthView.h"
#include "KngSynthPresets.h"
#include "KngPopOut.h"

namespace te = tracktion;

static const juce::Colour cyan (0xff3ee0ff), purple (0xffb56bff), steel (0xff9fb4da), pink (0xffff5fa8),
                          yellow (0xffffd23e), ice (0xffe6f7ff), cardTitle (0xff6d86b3);

static KngFaceStyle makeSynthStyle()
{
    KngFaceStyle s;
    s.accent    = cyan;
    s.bodyLight = juce::Colour (0xff24324d);
    s.bodyDark  = juce::Colour (0xff0c1322);
    s.pointer   = ice;
    s.track     = juce::Colours::white.withAlpha (0.07f);
    s.label     = juce::Colour (0xff8aa0c8);
    s.value     = juce::Colour (0xff9ff0ff);
    s.panel     = juce::Colour (0xff111b2e);
    s.outline   = juce::Colour (0xff1c2a44);
    s.labelFace = KngFonts::spaceGrotesk;
    s.valueFace = KngFonts::mono;
    s.menuFace  = KngFonts::mono;
    s.upperCaseLabels = false;
    return s;
}

// How each knob's value reads
static const char* unitFor (int p)
{
    using namespace KngSynth;

    switch (p)
    {
        case cutoff:                                    return "Hz";
        case lfoRate:                                   return "lfo";
        case master:                                    return "dB";
        case oscAFine: case oscBFine:                   return "ct";
        case fenvA: case fenvD: case fenvR:
        case ampA: case ampD: case ampR: case glide:    return "sec";
        case oscAOct: case oscBOct: case oscASemi:
        case oscBSemi: case oscAUnison: case oscBUnison: return "";
        default:                                        return "%";
    }
}

//==============================================================================
KngSynthView::KngSynthView (KngSynthPlugin& owner)
    : plugin (owner), pluginRef (&owner), style (makeSynthStyle()), look (style)
{
    setLookAndFeel (&look);

    juce::StringArray names;

    for (auto& preset : KngSynth::presets())
        names.add (preset.name);

    presetBox.attach (plugin, names, [this] (int i) { plugin.applyPreset (i); });
    presetBox.setTooltip ("Load a factory sound, then tweak it");
    addAndMakeVisible (presetBox);

    addAndMakeVisible (popButton);
    popButton.setTooltip ("Open in its own window");
    popButton.onClick = [this] { KngPopOut::open (plugin); };

    // Cards, in the order the sound travels
    cards.push_back ({ "OSC A",        cyan,   dispWaveA,     {}, {} });
    cards.push_back ({ "OSC B",        purple, dispWaveB,     {}, {} });
    cards.push_back ({ "SUB + NOISE",  steel,  dispNone,      {}, {} });
    cards.push_back ({ "FILTER",       pink,   dispFilter,    {}, {} });
    cards.push_back ({ "VOLUME SHAPE", cyan,   dispAmp,       {}, {} });
    cards.push_back ({ "FILTER SHAPE", pink,   dispFilterEnv, {}, {} });
    cards.push_back ({ "WOBBLE (LFO)", yellow, dispLfo,       {}, {} });
    cards.push_back ({ "OUTPUT",       ice,    dispNone,      {}, {} });

    using namespace KngSynth;

    addKnob (oscAOct, "Octave", cyan);    addKnob (oscASemi, "Semi", cyan);     addKnob (oscAFine, "Fine", cyan);
    addKnob (oscAUnison, "Voices", cyan); addKnob (oscADetune, "Detune", cyan); addKnob (oscALevel, "Level", cyan);

    addKnob (oscBOct, "Octave", purple);    addKnob (oscBSemi, "Semi", purple);     addKnob (oscBFine, "Fine", purple);
    addKnob (oscBUnison, "Voices", purple); addKnob (oscBDetune, "Detune", purple); addKnob (oscBLevel, "Level", purple);

    addKnob (subLevel, "Sub", steel); addKnob (noiseLevel, "Noise", steel); addKnob (spread, "Width", steel);

    addKnob (cutoff, "Cutoff", pink); addKnob (reso, "Reso", pink); addKnob (drive, "Drive", pink);
    addKnob (filterEnv, "Env", pink); addKnob (keyTrack, "Key", pink);

    addKnob (ampA, "Attack", cyan); addKnob (ampD, "Decay", cyan); addKnob (ampS, "Sustain", cyan); addKnob (ampR, "Release", cyan);
    addKnob (fenvA, "Attack", pink); addKnob (fenvD, "Decay", pink); addKnob (fenvS, "Sustain", pink); addKnob (fenvR, "Release", pink);

    addKnob (lfoRate, "Speed", yellow); addKnob (lfoPitch, "Vibrato", yellow); addKnob (lfoCutoff, "Wobble", yellow);

    addKnob (glide, "Glide", ice); addKnob (velSens, "Velocity", ice); addKnob (master, "Volume", ice);

    modeChoice       = makeChoice (voiceMode,  "POLY|MONO|LEGATO",   cyan);
    waveAChoice      = makeChoice (oscAWave,   "SAW|SQR|TRI|SINE",   cyan);
    waveBChoice      = makeChoice (oscBWave,   "SAW|SQR|TRI|SINE",   purple);
    filterTypeChoice = makeChoice (filterType, "LOW|HIGH|BAND",      pink);
    lfoShapeChoice   = makeChoice (lfoShape,   "SIN|TRI|SAW|SQR",    yellow);

    // Play it from the panel
    addAndMakeVisible (keyboard);
    keyboard.setAvailableRange (24, 96);
    keyboard.setLowestVisibleKey (48);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffdfe9f7));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff0c1322));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, cyan.withAlpha (0.7f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, cyan.withAlpha (0.25f));
    keyboardState.addListener (this);

    setSize (1100, 640);
    startTimerHz (15);
}

KngSynthView::~KngSynthView()
{
    stopTimer();
    keyboardState.removeListener (this);
    setLookAndFeel (nullptr);
}

void KngSynthView::addKnob (int param, const char* label, juce::Colour colour)
{
    const auto& in = KngSynth::info (param);
    const KngParamDef def { in.id, in.name, in.min, in.max, in.def, in.mid, in.step, unitFor (param), in.choices };

    auto k = std::make_unique<KngFaceKnob> (plugin, *plugin.getParam (param), def, style, label);
    k->labelSize = 10.5f;
    k->valueSize = 10.0f;
    k->setArcColour (colour);
    addAndMakeVisible (*k);
    knobs[param] = std::move (k);
}

std::unique_ptr<KngFaceChoice> KngSynthView::makeChoice (int param, const char* shortNames, juce::Colour colour)
{
    const auto& in = KngSynth::info (param);
    const KngParamDef def { in.id, in.name, in.min, in.max, in.def, in.mid, in.step, "", in.choices };

    auto c = std::make_unique<KngFaceChoice> (plugin, *plugin.getParam (param), def, style, shortNames);
    c->onColour = colour;
    addAndMakeVisible (*c);
    return c;
}

float KngSynthView::value (int param) const
{
    return plugin.getParam (param)->getCurrentValue();
}

//==============================================================================
void KngSynthView::timerCallback()
{
    popButton.setVisible (! KngPopOut::isPoppedOut (*this));

    // Screens follow the knobs, automation and presets
    for (auto& c : cards)
        if (! c.displayArea.isEmpty())
            repaint (c.displayArea);
}

void KngSynthView::handleNoteOn (juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity)
{
    if (auto* at = dynamic_cast<te::AudioTrack*> (plugin.getOwnerTrack()))
        at->playGuideNote (midiNoteNumber, te::MidiChannel (1), juce::roundToInt (velocity * 127.0f), false, true, false);
}

void KngSynthView::handleNoteOff (juce::MidiKeyboardState*, int, int, float)
{
    if (auto* at = dynamic_cast<te::AudioTrack*> (plugin.getOwnerTrack()))
        at->turnOffGuideNotes (te::MidiChannel (1));
}

//==============================================================================
void KngSynthView::layoutCard (Card& card, juce::Rectangle<int> area, KngFaceChoice* choice, bool choiceInTitle,
                               std::initializer_list<int> params)
{
    card.bounds = area;
    card.displayArea = {};

    auto c = area.reduced (12, 8);
    auto title = c.removeFromTop (20);

    if (choice != nullptr && choiceInTitle)
    {
        const int w = juce::jmin (150, title.getWidth() / 2);
        choice->setBounds (title.removeFromRight (w).withSizeKeepingCentre (w, 18));
    }

    c.removeFromTop (4);
    const int n = (int) params.size();

    // A narrow card with no screen stacks its knobs when there's room
    if (card.display == dispNone && n <= 3 && c.getWidth() < 170 && c.getHeight() >= n * 64)
    {
        const int cell = c.getHeight() / n;

        for (int p : params)
            knobs[p]->setBounds (c.removeFromTop (cell).withSizeKeepingCentre (juce::jmin (c.getWidth(), 90), cell));

        return;
    }

    const int knobRow = card.display != dispNone ? juce::jlimit (50, 84, c.getHeight() - 52) : juce::jmin (c.getHeight(), 96);
    auto row = c.removeFromBottom (knobRow);

    if (choice != nullptr && ! choiceInTitle)
    {
        c.removeFromBottom (4);
        choice->setBounds (c.removeFromBottom (18));
        c.removeFromBottom (6);
    }

    if (card.display != dispNone && c.getHeight() >= 20)
        card.displayArea = c;

    const int colW = row.getWidth() / juce::jmax (1, n);

    for (int p : params)
        knobs[p]->setBounds (row.removeFromLeft (colW).reduced (1, 0));
}

void KngSynthView::resized()
{
    using namespace KngSynth;

    panel = getLocalBounds().reduced (8);
    auto inner = panel.reduced (18, 12);

    header = inner.removeFromTop (40);
    {
        auto h = header;
        popButton.setBounds (h.removeFromRight (32).withSizeKeepingCentre (32, 32));
        h.removeFromRight (10);
        presetBox.setBounds (h.removeFromRight (200).withSizeKeepingCentre (200, 32));
        modeChoice->setBounds (header.getX() + 170, header.getCentreY() - 11, 190, 22);
    }

    inner.removeFromTop (10);

    // The keyboard shows when there's room (pop the panel out to always see it)
    const bool showKeys = inner.getHeight() >= 430;
    keyboard.setVisible (showKeys);

    if (showKeys)
    {
        keyboard.setBounds (inner.removeFromBottom (44));
        inner.removeFromBottom (10);
    }

    auto row1 = inner.removeFromTop ((int) (inner.getHeight() * 0.56f));
    inner.removeFromTop (10);
    auto row2 = inner;

    auto split = [] (juce::Rectangle<int> row, std::initializer_list<float> parts)
    {
        std::vector<juce::Rectangle<int>> out;
        float total = 0.0f;

        for (auto p : parts)
            total += p;

        const int gap = 10;
        const float usable = (float) (row.getWidth() - gap * ((int) parts.size() - 1));

        for (auto p : parts)
        {
            out.push_back (row.removeFromLeft (juce::roundToInt (usable * p / total)));
            row.removeFromLeft (gap);
        }

        return out;
    };

    const auto r1 = split (row1, { 1.25f, 1.25f, 0.6f, 1.35f });
    const auto r2 = split (row2, { 1.15f, 1.15f, 1.0f, 0.8f });

    layoutCard (cards[0], r1[0], waveAChoice.get(), false, { oscAOct, oscASemi, oscAFine, oscAUnison, oscADetune, oscALevel });
    layoutCard (cards[1], r1[1], waveBChoice.get(), false, { oscBOct, oscBSemi, oscBFine, oscBUnison, oscBDetune, oscBLevel });
    layoutCard (cards[2], r1[2], nullptr, false, { subLevel, noiseLevel, spread });
    layoutCard (cards[3], r1[3], filterTypeChoice.get(), true, { cutoff, reso, drive, filterEnv, keyTrack });

    layoutCard (cards[4], r2[0], nullptr, false, { ampA, ampD, ampS, ampR });
    layoutCard (cards[5], r2[1], nullptr, false, { fenvA, fenvD, fenvS, fenvR });
    layoutCard (cards[6], r2[2], lfoShapeChoice.get(), false, { lfoRate, lfoPitch, lfoCutoff });
    layoutCard (cards[7], r2[3], nullptr, false, { glide, velSens, master });
}

//==============================================================================
void KngSynthView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff070b14));

    const auto p = panel.toFloat();
    juce::ColourGradient bg (juce::Colour (0xff14213b), p.getX() + p.getWidth() * 0.2f, p.getY(),
                             juce::Colour (0xff080d18), p.getRight(), p.getBottom(), true);
    bg.addColour (0.55, juce::Colour (0xff0b1220));
    g.setGradientFill (bg);
    g.fillRoundedRectangle (p, 18.0f);
    g.setColour (juce::Colour (0xff1c2a44));
    g.drawRoundedRectangle (p.reduced (0.5f), 18.0f, 1.0f);

    // KNG Synth, with a glow on Synth
    const auto h = header.toFloat();
    g.setFont (KngFonts::font (KngFonts::spaceGrotesk, 26.0f));
    g.setColour (ice);
    g.drawText ("KNG", juce::Rectangle<float> (h.getX(), h.getY(), 70.0f, h.getHeight()), juce::Justification::centredLeft, false);

    const juce::Rectangle<float> word (h.getX() + 62.0f, h.getY(), 110.0f, h.getHeight());
    g.setColour (cyan.withAlpha (0.18f));

    for (auto offset : { juce::Point<float> (-2.0f, 0.0f), { 2.0f, 0.0f }, { 0.0f, -2.0f }, { 0.0f, 2.0f } })
        g.drawText ("Synth", word.translated (offset.x, offset.y), juce::Justification::centredLeft, false);

    g.setColour (cyan);
    g.drawText ("Synth", word, juce::Justification::centredLeft, false);

    for (auto& c : cards)
        paintCard (g, c);
}

void KngSynthView::paintCard (juce::Graphics& g, const Card& card)
{
    using namespace KngSynth;

    if (card.bounds.isEmpty())
        return;

    const auto b = card.bounds.toFloat();
    juce::ColourGradient fill (cyan.withAlpha (0.06f), 0.0f, b.getY(), cyan.withAlpha (0.02f), 0.0f, b.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (juce::Colour (0xff1c2a44));
    g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);

    // Title with its color square
    g.setColour (card.colour);
    g.fillRoundedRectangle (b.getX() + 12.0f, b.getY() + 14.0f, 8.0f, 8.0f, 2.0f);
    g.setFont (KngFonts::font (KngFonts::spaceGrotesk, 11.0f, 0.18f));
    g.setColour (cardTitle);
    g.drawText (card.name, juce::Rectangle<float> (b.getX() + 26.0f, b.getY() + 8.0f, b.getWidth() - 40.0f, 20.0f),
                juce::Justification::centredLeft, false);

    if (card.displayArea.isEmpty())
        return;

    const auto d = card.displayArea.toFloat();
    g.setColour (juce::Colour (0xff060a12));
    g.fillRoundedRectangle (d, 8.0f);
    g.setColour (juce::Colour (0xff182540));
    g.drawRoundedRectangle (d.reduced (0.5f), 8.0f, 1.0f);

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (d.reduced (1.0f).toNearestInt());
    const auto plot = d.reduced (8.0f, 6.0f);

    switch (card.display)
    {
        case dispWaveA:
            paintWave (g, plot, card.colour, juce::roundToInt (value (oscAWave)), juce::roundToInt (value (oscAUnison)), value (oscADetune));
            break;

        case dispWaveB:
            paintWave (g, plot, card.colour, juce::roundToInt (value (oscBWave)), juce::roundToInt (value (oscBUnison)), value (oscBDetune));
            break;

        case dispFilter:    paintFilter (g, plot, card.colour); break;
        case dispAmp:       paintEnvelope (g, plot, card.colour, value (ampA), value (ampD), value (ampS), value (ampR)); break;
        case dispFilterEnv: paintEnvelope (g, plot, card.colour, value (fenvA), value (fenvD), value (fenvS), value (fenvR)); break;
        case dispLfo:       paintLfo (g, plot, card.colour); break;
        case dispNone:
        default:            break;
    }
}

//==============================================================================
static float waveSample (int wave, double ph)
{
    ph -= std::floor (ph);

    switch (wave)
    {
        case 0:  return (float) (2.0 * ph - 1.0);                         // Saw
        case 1:  return ph < 0.5 ? 1.0f : -1.0f;                          // Square
        case 2:  return (float) (1.0 - 4.0 * std::abs (ph - 0.5));        // Triangle
        default: return (float) std::sin (ph * juce::MathConstants<double>::twoPi);   // Sine
    }
}

void KngSynthView::paintWave (juce::Graphics& g, juce::Rectangle<float> a, juce::Colour colour, int wave, int voices, float detune)
{
    // Extra voices show as faint copies, spread by Detune
    const int shown = juce::jlimit (1, 3, voices);

    for (int u = shown - 1; u >= 0; --u)
    {
        juce::Path path;
        const double offset = u * (0.04 + detune * 0.12);

        for (int k = 0; k <= 240; ++k)
        {
            const double ph = 2.5 * k / 240.0 + offset;
            const float x = a.getX() + a.getWidth() * (float) k / 240.0f;
            const float y = a.getCentreY() - waveSample (wave, ph) * a.getHeight() * 0.42f;

            if (k == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }

        if (u == 0)
        {
            g.setColour (colour.withAlpha (0.25f));
            g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour (colour.withAlpha (0.3f));
            g.strokePath (path, juce::PathStrokeType (1.2f));
        }
    }
}

void KngSynthView::paintFilter (juce::Graphics& g, juce::Rectangle<float> a, juce::Colour colour)
{
    using namespace KngSynth;

    const double fc = juce::jmax (20.0f, value (cutoff));
    const double q = 1.0 / juce::jmax (0.04, 2.0 - 1.96 * (double) value (reso));
    const int type = juce::roundToInt (value (filterType));
    constexpr double topDb = 18.0, bottomDb = -36.0;

    auto toY = [&] (double db)
    {
        db = juce::jlimit (bottomDb, topDb, db);
        return a.getY() + (float) ((topDb - db) / (topDb - bottomDb)) * a.getHeight();
    };

    auto dbAt = [&] (double f)
    {
        const double w = f / fc;
        const double den = std::sqrt ((1.0 - w * w) * (1.0 - w * w) + (w / q) * (w / q));
        const double mag = type == 0 ? 1.0 / den : type == 1 ? (w * w) / den : (w / q) / den;
        return 20.0 * std::log10 (juce::jmax (1.0e-6, mag));
    };

    juce::Path curve;

    for (int k = 0; k <= 160; ++k)
    {
        const double f = 20.0 * std::pow (1000.0, k / 160.0);
        const float x = a.getX() + a.getWidth() * (float) k / 160.0f;
        const float y = toY (dbAt (f));

        if (k == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    auto fill = curve;
    fill.lineTo (a.getRight(), a.getBottom() + 6.0f);
    fill.lineTo (a.getX(), a.getBottom() + 6.0f);
    fill.closeSubPath();

    juce::ColourGradient shade (colour.withAlpha (0.35f), 0.0f, a.getY(), colour.withAlpha (0.0f), 0.0f, a.getBottom(), false);
    g.setGradientFill (shade);
    g.fillPath (fill);

    g.setColour (colour.withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colour);
    g.strokePath (curve, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // A dot at the cutoff
    const float dotX = a.getX() + a.getWidth() * (float) (std::log (fc / 20.0) / std::log (1000.0));
    const auto dot = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ dotX, toY (dbAt (fc)) });
    g.setColour (juce::Colour (0xff0b1220));
    g.fillEllipse (dot);
    g.setColour (juce::Colours::white);
    g.drawEllipse (dot, 2.0f);
}

void KngSynthView::paintEnvelope (juce::Graphics& g, juce::Rectangle<float> a, juce::Colour colour,
                                  float attack, float decay, float sustain, float release)
{
    // Square roots keep short times visible next to long ones
    const float wa = std::sqrt (attack) + 0.05f, wd = std::sqrt (decay) + 0.05f, wh = 0.35f, wr = std::sqrt (release) + 0.05f;
    const float scale = a.getWidth() / (wa + wd + wh + wr);

    const float x0 = a.getX(), x1 = x0 + wa * scale, x2 = x1 + wd * scale, x3 = x2 + wh * scale, x4 = x3 + wr * scale;
    const float top = a.getY() + 2.0f, bottom = a.getBottom() - 1.0f;
    const float sy = top + (1.0f - juce::jlimit (0.0f, 1.0f, sustain)) * (bottom - top);

    juce::Path shape;
    shape.startNewSubPath (x0, bottom);
    shape.lineTo (x1, top);
    shape.lineTo (x2, sy);
    shape.lineTo (x3, sy);
    shape.lineTo (x4, bottom);

    auto fill = shape;
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.12f));
    g.fillPath (fill);

    g.setColour (colour);
    g.strokePath (shape, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (auto pt : { juce::Point<float> (x1, top), { x2, sy }, { x3, sy } })
    {
        const auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre (pt);
        g.setColour (juce::Colour (0xff0b1220));
        g.fillEllipse (dot);
        g.setColour (juce::Colours::white);
        g.drawEllipse (dot, 1.6f);
    }
}

void KngSynthView::paintLfo (juce::Graphics& g, juce::Rectangle<float> a, juce::Colour colour)
{
    using namespace KngSynth;

    // LFO shapes: Sine, Triangle, Saw, Square. Faster speeds draw more cycles.
    const int shape = juce::roundToInt (value (lfoShape));
    const int asWave = shape == 0 ? 3 : shape == 1 ? 2 : shape == 2 ? 0 : 1;
    const float cycles = juce::jlimit (1.0f, 8.0f, 1.0f + value (lfoRate) * 0.35f);

    juce::Path path;

    for (int k = 0; k <= 240; ++k)
    {
        const double ph = cycles * k / 240.0 + 0.25;
        const float x = a.getX() + a.getWidth() * (float) k / 240.0f;
        const float y = a.getCentreY() - waveSample (asWave, ph) * a.getHeight() * 0.42f;

        if (k == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    g.setColour (colour.withAlpha (0.25f));
    g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
