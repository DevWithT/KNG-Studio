#include "KngModView.h"
#include "KngModPlugin.h"

namespace te = tracktion;

KngModView::KngModView (KngModPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xffa78bfa, 0xff322a4a, 0xff100d1a, 0xfff4f0ff, 0xffaaa0cc, 0xffddd6fe, 0xff14101f, 0xff322a4a,
                                 KngFonts::sora, false),
                   { juce::Colour (0xff08060d), juce::Colour (0xff1c1630), juce::Colour (0xff0d0a16), juce::Colour (0xff2c2446) }),
      fx (owner)
{
    titleWidth = 160;
    sideWidth = 50;
    maxKnobWidth = 110;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    addHeaderChoice (*fx.getParam (KngModPlugin::mode), 0.0f, "Chorus|Flanger|Phaser", 280);

    auto& rateKnob = addKnob (fx, KngModPlugin::rate, "Rate");
    rateKnob.formatter = [] (float v) { return juce::String (v, 2) + " Hz"; };

    addKnob (fx, KngModPlugin::depth, "Depth");
    addKnob (fx, KngModPlugin::feedback, "Feedback");
    addKnob (fx, KngModPlugin::width, "Width");
    addKnob (fx, KngModPlugin::mix, "Mix");
}

float KngModView::value (int p) const
{
    return fx.getParam (p)->getCurrentValue();
}

void KngModView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);
    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);
}

void KngModView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "Mod", KngFonts::sora, 30.0f);
}

void KngModView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngModView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (area.reduced (1.0f).toNearestInt());

    const auto plot = area.reduced (18.0f, 16.0f).withTrimmedTop (16.0f);
    const double phase = fx.getPhase();
    const float depthAmount = value (KngModPlugin::depth);
    const double offset = value (KngModPlugin::width) * 0.25;
    const float level = juce::jlimit (0.0f, 1.0f, (outDb + 40.0f) / 40.0f);
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    // Two cycles of each side's sweep, scrolling as it moves; right side a little behind the left
    for (int side = 0; side < 2; ++side)
    {
        juce::Path wave;
        const double start = phase + (side == 1 ? offset : 0.0);

        for (int k = 0; k <= 160; ++k)
        {
            const float t = (float) k / 160.0f;
            const float x = plot.getX() + t * plot.getWidth();
            const float y = plot.getCentreY() - (float) std::sin ((start + t * 2.0) * twoPi) * plot.getHeight() * 0.42f * (0.2f + 0.8f * depthAmount);

            if (k == 0)
                wave.startNewSubPath (x, y);
            else
                wave.lineTo (x, y);
        }

        const auto colour = side == 0 ? style.accent : juce::Colour (0xfff0abfc);
        g.setColour (colour.withAlpha (0.2f + 0.2f * level));
        g.strokePath (wave, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
    g.setColour (style.accent);
    g.drawText ("L", juce::Rectangle<float> (plot.getRight() - 30.0f, plot.getY(), 12.0f, 12.0f), juce::Justification::centred, false);
    g.setColour (juce::Colour (0xfff0abfc));
    g.drawText ("R", juce::Rectangle<float> (plot.getRight() - 14.0f, plot.getY(), 12.0f, 12.0f), juce::Justification::centred, false);

    const int m = juce::roundToInt (value (KngModPlugin::mode));
    const char* names[] = { "CHORUS", "FLANGER", "PHASER" };
    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText (juce::String (names[juce::jlimit (0, 2, m)]) + "   " + juce::String (value (KngModPlugin::rate), 2) + " Hz",
                area.reduced (14.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, false);
}
