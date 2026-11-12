#include "KngDoublerView.h"
#include "KngDoublerPlugin.h"

namespace te = tracktion;

// Where each double sits and how it drifts (matches the Doubler's own voices)
struct DoubleSpot { float pan; double rate, phase; };

static const DoubleSpot spots[4] = {
    { -1.0f, 0.43, 0.00 }, { 1.0f, 0.57, 0.50 }, { -0.6f, 0.37, 0.25 }, { 0.6f, 0.71, 0.75 }
};

KngDoublerView::KngDoublerView (KngDoublerPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xff38bdf8, 0xff1f3a52, 0xff0a1622, 0xffeef9ff, 0xff8fb8d6, 0xff8fdcff, 0xff0b1826, 0xff1d3a52,
                                 KngFonts::spaceGrotesk, false),
                   { juce::Colour (0xff050b10), juce::Colour (0xff0f2234), juce::Colour (0xff07111c), juce::Colour (0xff1a3550) }),
      fx (owner)
{
    titleWidth = 200;
    sideWidth = 50;
    maxKnobWidth = 110;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    addHeaderChoice (*fx.getParam (KngDoublerPlugin::voices), 0.0f, "2 Voices|4 Voices", 190);

    addKnob (fx, KngDoublerPlugin::detune, "Detune");
    addKnob (fx, KngDoublerPlugin::delay, "Delay");
    addKnob (fx, KngDoublerPlugin::width, "Width");
    addKnob (fx, KngDoublerPlugin::lowCut, "Low Cut");
    addKnob (fx, KngDoublerPlugin::mix, "Mix");
}

float KngDoublerView::value (int p) const
{
    return fx.getParam (p)->getCurrentValue();
}

void KngDoublerView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);
    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);
    clock += 1.0 / 30.0;
}

void KngDoublerView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "Doubler", KngFonts::spaceGrotesk, 28.0f);
}

void KngDoublerView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngDoublerView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (area.reduced (1.0f).toNearestInt());

    const auto plot = area.reduced (20.0f, 16.0f).withTrimmedTop (16.0f);
    const float cx = plot.getCentreX(), cy = plot.getCentreY();
    const float halfW = plot.getWidth() * 0.42f;
    const int numVoices = value (KngDoublerPlugin::voices) > 0.5f ? 4 : 2;
    const float widthAmt = value (KngDoublerPlugin::width);
    const float detune = value (KngDoublerPlugin::detune);
    const float mixAmt = value (KngDoublerPlugin::mix);
    const float level = juce::jlimit (0.0f, 1.0f, (outDb + 40.0f) / 40.0f);
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    // The stereo field, left to right
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine ((int) cy, cx - halfW, cx + halfW);

    for (float p : { -1.0f, -0.5f, 0.5f, 1.0f })
        g.drawVerticalLine ((int) (cx + p * halfW), cy - 6.0f, cy + 6.0f);

    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    g.setColour (style.label);
    g.drawText ("L", juce::Rectangle<float> (cx - halfW - 26.0f, cy - 8.0f, 16.0f, 16.0f), juce::Justification::centred, false);
    g.drawText ("R", juce::Rectangle<float> (cx + halfW + 10.0f, cy - 8.0f, 16.0f, 16.0f), juce::Justification::centred, false);

    // The doubles drift a little, more with more Detune
    const float size = 16.0f + mixAmt * 16.0f;
    const float driftPx = 4.0f + detune * 0.8f;

    for (int v = 0; v < numVoices; ++v)
    {
        const auto& s = spots[v];
        const float wobble = (float) std::sin ((clock * s.rate + s.phase) * twoPi);
        const float x = cx + s.pan * widthAmt * halfW + wobble * driftPx;
        const float y = cy + (v % 2 == 0 ? -1.0f : 1.0f) * (plot.getHeight() * 0.22f) + wobble * driftPx * 0.5f;
        const juce::Point<float> pos (x, y);

        g.setColour (style.accent.withAlpha (0.12f));
        g.drawLine (juce::Line<float> ({ cx, cy }, pos), 1.0f);

        const float glowR = size * (1.1f + 0.6f * level);
        juce::ColourGradient glow (style.accent.withAlpha (0.45f), x, y, style.accent.withAlpha (0.0f), x + glowR, y, true);
        g.setGradientFill (glow);
        g.fillEllipse (juce::Rectangle<float> (glowR * 2.0f, glowR * 2.0f).withCentre (pos));

        g.setColour (style.accent.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (pos));
    }

    // Your vocal, in the middle
    juce::ColourGradient centreGlow (juce::Colours::white.withAlpha (0.5f), cx, cy, juce::Colours::white.withAlpha (0.0f), cx + 40.0f, cy, true);
    g.setGradientFill (centreGlow);
    g.fillEllipse (juce::Rectangle<float> (80.0f, 80.0f).withCentre ({ cx, cy }));
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre ({ cx, cy }));

    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText (juce::String (numVoices) + " VOICES   " + juce::String (juce::roundToInt (detune)) + " ct   "
                    + juce::String (juce::roundToInt (value (KngDoublerPlugin::delay))) + " ms",
                area.reduced (14.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, false);
}
