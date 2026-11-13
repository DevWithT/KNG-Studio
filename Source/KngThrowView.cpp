#include "KngThrowView.h"
#include "KngThrowPlugin.h"

namespace te = tracktion;

KngThrowView::KngThrowView (KngThrowPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xfffde047, 0xff45401c, 0xff17150a, 0xfffffbe0, 0xffc9c08a, 0xfffef08a, 0xff1b190c, 0xff45401c,
                                 KngFonts::spaceGrotesk, true),
                   { juce::Colour (0xff0b0a04), juce::Colour (0xff27240f), juce::Colour (0xff121107), juce::Colour (0xff3d3818) }),
      fx (owner)
{
    titleWidth = 190;
    sideWidth = 50;
    maxKnobWidth = 110;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    addHeaderChoice (*fx.getParam (KngThrowPlugin::time), 0.0f, KngThrowPlugin::timeNames().joinIntoString ("|"), 360);
    addHeaderSwitch (*fx.getParam (KngThrowPlugin::pingPong), "Ping-Pong", "Echoes bounce between left and right", 130);

    addKnob (fx, KngThrowPlugin::feedback, "Feedback");
    addKnob (fx, KngThrowPlugin::duck, "Duck");

    auto& toneKnob = addKnob (fx, KngThrowPlugin::tone, "Tone");
    toneKnob.formatter = [] (float v) { return juce::String (v / 1000.0f, 1) + " kHz"; };

    addKnob (fx, KngThrowPlugin::wash, "Wash");
    addKnob (fx, KngThrowPlugin::mix, "Mix");
}

float KngThrowView::value (int p) const
{
    return fx.getParam (p)->getCurrentValue();
}

void KngThrowView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);
    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);
    duckDb = gr > duckDb ? gr : duckDb * 0.88f;
    clock += 1.0 / 30.0;
}

void KngThrowView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "THROW", KngFonts::anton, 32.0f);
}

void KngThrowView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngThrowView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (area.reduced (1.0f).toNearestInt());

    const auto plot = area.reduced (20.0f, 16.0f).withTrimmedTop (18.0f);
    const float cy = plot.getCentreY();
    const int t = juce::roundToInt (value (KngThrowPlugin::time));
    const float beats = KngThrowPlugin::beatsFor (t);
    const float fb = value (KngThrowPlugin::feedback);
    const bool ping = value (KngThrowPlugin::pingPong) > 0.5f;
    const float wash = value (KngThrowPlugin::wash);

    // Two bars across the screen, beat lines faint
    const float pxPerBeat = plot.getWidth() / 8.0f;

    for (int b = 0; b <= 8; ++b)
    {
        g.setColour (juce::Colours::white.withAlpha (b % 4 == 0 ? 0.12f : 0.05f));
        g.drawVerticalLine ((int) (plot.getX() + b * pxPerBeat), plot.getY(), plot.getBottom());
    }

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine ((int) cy, plot.getX(), plot.getRight());

    // While it's ducking, the echoes dim
    const float duckShade = 1.0f - juce::jlimit (0.0f, 0.8f, duckDb / 24.0f);
    const float bob = 0.5f + 0.5f * (float) std::sin (clock * 4.0);

    // The sound you sang
    const float inLevel = juce::jlimit (0.0f, 1.0f, (inDb + 50.0f) / 50.0f);
    const float srcSize = 14.0f + 10.0f * inLevel;
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (srcSize, srcSize).withCentre ({ plot.getX() + 8.0f, cy }));

    // Each repeat: further along, quieter, softer with Wash
    float amp = 1.0f;
    const float yOffset = plot.getHeight() * 0.3f;

    for (int k = 1; k <= 24; ++k)
    {
        amp *= k == 1 ? 1.0f : juce::jmax (0.0f, fb);

        const float x = plot.getX() + 8.0f + k * beats * pxPerBeat;

        if (x > plot.getRight() || amp < 0.03f)
            break;

        const float y = cy + (ping ? (k % 2 == 1 ? -yOffset : yOffset) : 0.0f);
        const float size = 6.0f + 18.0f * amp;
        const float alpha = (0.25f + 0.7f * amp) * duckShade;

        // A soft halo for Wash
        if (wash > 0.01f)
        {
            const float halo = size * (1.0f + wash * 1.6f);
            juce::ColourGradient glow (style.accent.withAlpha (alpha * 0.5f * wash), x, y, style.accent.withAlpha (0.0f), x + halo, y, true);
            g.setGradientFill (glow);
            g.fillEllipse (juce::Rectangle<float> (halo * 2.0f, halo * 2.0f).withCentre ({ x, y }));
        }

        g.setColour (style.accent.withAlpha (alpha * (0.85f + 0.15f * bob)));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ x, y }));
    }

    // Readout: note length, tempo, milliseconds
    const float bpm = juce::jmax (1.0f, fx.getBpm());
    const int ms = juce::roundToInt (beats * 60000.0f / bpm);

    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    auto top = area.reduced (14.0f, 8.0f).removeFromTop (16.0f);
    g.drawText (KngThrowPlugin::timeNames()[t] + "   " + juce::String (juce::roundToInt (bpm)) + " BPM   " + juce::String (ms) + " ms",
                top, juce::Justification::centredLeft, false);

    if (duckDb > 1.0f)
    {
        g.setFont (KngFonts::font (KngFonts::spaceGrotesk, 12.0f, 0.2f));
        g.setColour (style.accent);
        g.drawText ("DUCKING -" + juce::String (juce::roundToInt (duckDb)) + " dB", top, juce::Justification::centredRight, false);
    }
}
