#include "KngAirView.h"
#include "KngAirPlugin.h"

namespace te = tracktion;

KngAirView::KngAirView (KngAirPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xffbdf4ff, 0xff2a3a44, 0xff0d1418, 0xffffffff, 0xff9fb8c4, 0xffe6fbff, 0xff0f181d, 0xff2a3a44,
                                 KngFonts::sora, false),
                   { juce::Colour (0xff060a0c), juce::Colour (0xff15232b), juce::Colour (0xff0a1115), juce::Colour (0xff23363f) }),
      fx (owner)
{
    titleWidth = 170;
    sideWidth = 50;
    maxKnobWidth = 110;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    addKnob (fx, KngAirPlugin::air, "Air");

    auto& freqKnob = addKnob (fx, KngAirPlugin::freq, "Frequency");
    freqKnob.formatter = [] (float v) { return juce::String (v / 1000.0f, 1) + " kHz"; };

    addKnob (fx, KngAirPlugin::presence, "Presence");
    addKnob (fx, KngAirPlugin::shine, "Shine");
    addKnob (fx, KngAirPlugin::output, "Output");
}

float KngAirView::value (int p) const
{
    return fx.getParam (p)->getCurrentValue();
}

void KngAirView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);
    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);
    clock += 1.0 / 30.0;
}

void KngAirView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "AIR", KngFonts::anton, 34.0f);
}

void KngAirView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngAirView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (area.reduced (1.0f).toNearestInt());

    const auto plot = area.reduced (16.0f, 14.0f).withTrimmedTop (14.0f);
    auto xOf = [&plot] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f); };
    auto yOf = [&plot] (float db) { return plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 6.0f) / 24.0f); };

    // Grid: 100 Hz, 1 kHz, 10 kHz and the 0 dB line
    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));

    const float gridHz[] = { 100.0f, 1000.0f, 10000.0f };
    const char* gridText[] = { "100", "1k", "10k" };

    for (int k = 0; k < 3; ++k)
    {
        const float hz = gridHz[k];
        const char* text = gridText[k];

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) xOf (hz), plot.getY(), plot.getBottom());
        g.setColour (style.label.withAlpha (0.7f));
        g.drawText (text, juce::Rectangle<float> (xOf (hz) + 3.0f, plot.getBottom() - 13.0f, 30.0f, 12.0f), juce::Justification::left, false);
    }

    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine ((int) yOf (0.0f), plot.getX(), plot.getRight());

    // Sparkles over the air band: more with Shine, brighter with the music
    const float shineAmount = value (KngAirPlugin::shine);
    const float level = juce::jlimit (0.0f, 1.0f, (outDb + 40.0f) / 40.0f);
    const float bandX = xOf (value (KngAirPlugin::freq) * 0.5f);
    const int count = 10 + (int) (shineAmount * 40.0f);
    juce::Random rng (7);

    for (int k = 0; k < count; ++k)
    {
        const float x = bandX + rng.nextFloat() * (plot.getRight() - bandX);
        const float y = plot.getY() + rng.nextFloat() * plot.getHeight() * 0.7f;
        const float phase = rng.nextFloat() * 6.283f;
        const float speed = 0.6f + rng.nextFloat();
        const float twinkle = 0.5f + 0.5f * std::sin ((float) (clock * speed * 3.0) + phase);
        const float r = 1.0f + 2.5f * twinkle;

        g.setColour (style.accent.withAlpha (twinkle * (0.15f + 0.6f * level) * (0.3f + 0.7f * shineAmount)));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre ({ x, y }));
    }

    // The lift curve
    juce::Path curve;

    for (int k = 0; k <= 120; ++k)
    {
        const float hz = 20.0f * std::pow (1000.0f, (float) k / 120.0f);
        const float x = xOf (hz), y = yOf (fx.curveAt (hz));

        if (k == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    auto fill = curve;
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();

    juce::ColourGradient grad (style.accent.withAlpha (0.28f), 0.0f, plot.getY(), style.accent.withAlpha (0.0f), 0.0f, plot.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (fill);

    g.setColour (style.accent.withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (style.accent);
    g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Readout
    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText ("AIR +" + juce::String (value (KngAirPlugin::air), 1) + " dB  @ "
                    + juce::String (value (KngAirPlugin::freq) / 1000.0f, 1) + " kHz",
                area.reduced (14.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, false);
}
