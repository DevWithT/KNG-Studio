#include "KngSynthView.h"
#include "KngSynthPresets.h"
#include "KngPopOut.h"

namespace te = tracktion;

static const juce::Colour cyan (0xff3ee0ff), purple (0xffb56bff), steel (0xff9fb4da), pink (0xffff5fa8),
                          yellow (0xffffd23e), ice (0xffe6f7ff), cardTitle (0xff6d86b3);

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
