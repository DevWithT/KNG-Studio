#include "KngPumpView.h"
#include "KngPumpPlugin.h"

namespace te = tracktion;

KngPumpView::KngPumpView (KngPumpPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xff10b981, 0xff1d3d32, 0xff0a1612, 0xffe6fff5, 0xff8fbfab, 0xff6ee7b7, 0xff0c1a15, 0xff1d3d32,
                                 KngFonts::barlowSemi, true),
                   { juce::Colour (0xff040a08), juce::Colour (0xff0f2219), juce::Colour (0xff07110d), juce::Colour (0xff1a3a2d) }),
      fx (owner)
{
    titleWidth = 180;
    sideWidth = 50;
    maxKnobWidth = 110;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    addHeaderChoice (*fx.getParam (KngPumpPlugin::rate), 0.0f, KngPumpPlugin::rateNames().joinIntoString ("|"), 300);

    addKnob (fx, KngPumpPlugin::depth, "Depth");
    addKnob (fx, KngPumpPlugin::recovery, "Recovery");
    addKnob (fx, KngPumpPlugin::curve, "Curve");
    addKnob (fx, KngPumpPlugin::offset, "Offset");
}

void KngPumpView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);
    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);
}

void KngPumpView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "PUMP", KngFonts::barlowHeavy, 36.0f);
}

void KngPumpView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngPumpView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (area.reduced (1.0f).toNearestInt());

    const auto plot = area.reduced (18.0f, 16.0f).withTrimmedTop (16.0f);
    const float cycleW = plot.getWidth() * 0.5f;
    auto yOf = [&plot] (float gain) { return plot.getBottom() - gain * plot.getHeight(); };

    // Beat markers at each cycle start
    for (int k = 0; k <= 2; ++k)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawVerticalLine ((int) (plot.getX() + k * cycleW), plot.getY(), plot.getBottom());
    }

    // Two cycles of the volume curve
    juce::Path curve;

    for (int k = 0; k <= 200; ++k)
    {
        const float t = (float) k / 100.0f;   // 0 to 2 cycles
        const float x = plot.getX() + t * cycleW;
        const float y = yOf (fx.shapeAt (t - std::floor (t)));

        if (k == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    auto fill = curve;
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();

    juce::ColourGradient grad (style.accent.withAlpha (0.3f), 0.0f, plot.getY(), style.accent.withAlpha (0.02f), 0.0f, plot.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (fill);
    g.setColour (style.accent.withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (style.accent);
    g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // The dot rides the first cycle in time with the music
    const float pos = fx.getCyclePos();
    const juce::Point<float> dot (plot.getX() + pos * cycleW, yOf (fx.shapeAt (pos)));
    juce::ColourGradient glow (juce::Colours::white.withAlpha (0.6f), dot.x, dot.y, juce::Colours::white.withAlpha (0.0f), dot.x + 18.0f, dot.y, true);
    g.setGradientFill (glow);
    g.fillEllipse (juce::Rectangle<float> (36.0f, 36.0f).withCentre (dot));
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (dot));

    const int r = juce::roundToInt (fx.getParam (KngPumpPlugin::rate)->getCurrentValue());
    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText ("EVERY " + KngPumpPlugin::rateNames()[r].toUpperCase() + "   DEPTH "
                    + juce::String (juce::roundToInt (fx.getParam (KngPumpPlugin::depth)->getCurrentValue() * 100.0f)) + "%",
                area.reduced (14.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, false);
}
