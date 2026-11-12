#include "KngSatView.h"
#include "KngSatPlugin.h"

namespace te = tracktion;

static KngParamDef satDef (int p, const char* unit)
{
    const auto& in = KngSat::info (p);
    return { "", "", (float) in.min, (float) in.max, (float) in.def, (float) in.mid, (float) in.step, unit, nullptr };
}

//==============================================================================
KngSatView::KngSatView (KngSatPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xffff7a45, 0xff4a2a1a, 0xff1a0d07, 0xfffff0e6, 0xffc99a80, 0xffffae85, 0xff1e0f08, 0xff4a2a1a,
                                 KngFonts::barlowSemi, true),
                   { juce::Colour (0xff0c0604), juce::Colour (0xff2a140a), juce::Colour (0xff120804), juce::Colour (0xff3a1e10) }),
      sat (owner)
{
    titleWidth = 250;
    sideWidth = 50;
    maxKnobWidth = 120;

    juce::StringArray names;

    for (auto& preset : KngSat::presets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { sat.applyPreset (i); });

    addHeaderChoice (*sat.getParam (KngSat::mode), (float) KngSat::info (KngSat::mode).min,
                     KngSat::modeNames().joinIntoString ("|"), 330);
    addHeaderSwitch (*sat.getParam (KngSat::autoGain), "Auto Gain",
                     "Keeps the output as loud as the input, so you hear the color, not just more volume", 130);

    auto& driveKnob = addKnob (*sat.getParam (KngSat::drive), satDef (KngSat::drive, ""), "Drive");
    driveKnob.formatter = [] (float v) { return juce::String (v, 1) + " dB"; };

    auto& toneKnob = addKnob (*sat.getParam (KngSat::tone), satDef (KngSat::tone, ""), "Tone");
    toneKnob.formatter = [] (float v)
    {
        return std::abs (v) < 0.01f ? juce::String ("Neutral")
             : v > 0.0f ? "Bright " + juce::String (juce::roundToInt (v * 100.0f)) + "%"
                        : "Dark " + juce::String (juce::roundToInt (-v * 100.0f)) + "%";
    };

    addKnob (*sat.getParam (KngSat::mix), satDef (KngSat::mix, "%"), "Mix");
    addKnob (*sat.getParam (KngSat::output), satDef (KngSat::output, "dB"), "Output");
}

float KngSatView::value (int p) const
{
    return sat.getParam (p)->getCurrentValue();
}

void KngSatView::tick()
{
    float in = 0.0f, out = 0.0f;
    sat.takeMeters (in, out);
    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);

    // The tube heats with Drive and flickers with the music
    const float heat = juce::jlimit (0.0f, 1.0f, value (KngSat::drive) / 24.0f);
    const float level = juce::jlimit (0.0f, 1.0f, (outDb + 40.0f) / 40.0f);
    const float target = 0.12f + heat * (0.35f + 0.65f * level);
    glow += (target - glow) * 0.25f;
}

//==============================================================================
void KngSatView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "SATURATOR", KngFonts::barlowHeavy, 34.0f);
}

void KngSatView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngSatView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    auto inner = area.reduced (12.0f);
    auto tube = inner.removeFromLeft (inner.getWidth() * 0.36f);
    inner.removeFromLeft (12.0f);

    paintTube (g, tube);
    paintCurve (g, inner);
}

void KngSatView::paintTube (juce::Graphics& g, juce::Rectangle<float> a)
{
    if (a.getHeight() < 60.0f)
        return;

    auto nameArea = a.removeFromBottom (24.0f);
    const auto glass = a.withSizeKeepingCentre (juce::jmin (90.0f, a.getWidth() * 0.5f), a.getHeight() * 0.92f);
    const auto c = glass.getCentre();
    const auto hot = style.accent.interpolatedWith (juce::Colour (0xffffe6b0), glow * 0.6f);

    // Glow behind the glass
    const float glowR = glass.getWidth() * (1.0f + glow * 0.8f);
    juce::ColourGradient halo (style.accent.withAlpha (0.55f * glow), c.x, c.y, style.accent.withAlpha (0.0f), c.x + glowR, c.y, true);
    g.setGradientFill (halo);
    g.fillEllipse (juce::Rectangle<float> (glowR * 2.0f, glowR * 2.0f).withCentre (c));

    // Glass
    const float corner = glass.getWidth() * 0.5f;
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRoundedRectangle (glass.withTrimmedBottom (16.0f), corner);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (glass.withTrimmedBottom (16.0f), corner, 1.5f);

    // Filament
    juce::Path filament;
    const float top = glass.getY() + glass.getHeight() * 0.22f, bottom = glass.getBottom() - glass.getHeight() * 0.28f;
    const float swing = glass.getWidth() * 0.18f;
    constexpr int zigs = 7;

    for (int k = 0; k <= zigs; ++k)
    {
        const float y = top + (bottom - top) * (float) k / (float) zigs;
        const float x = c.x + (k % 2 == 0 ? -swing : swing);

        if (k == 0)
            filament.startNewSubPath (x, y);
        else
            filament.lineTo (x, y);
    }

    g.setColour (hot.withAlpha (0.4f * glow + 0.1f));
    g.strokePath (filament, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (hot.withAlpha (0.5f + 0.5f * glow));
    g.strokePath (filament, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Base and pins
    const auto base = glass.withTrimmedTop (glass.getHeight() - 18.0f);
    g.setColour (juce::Colour (0xff2a1a12));
    g.fillRoundedRectangle (base, 4.0f);
    g.setColour (juce::Colour (0xff6b5446));

    for (int k = 0; k < 4; ++k)
        g.fillRect (base.getX() + base.getWidth() * (0.2f + 0.2f * k) - 1.0f, base.getBottom() - 2.0f, 2.0f, 6.0f);

    // The flavor's name under the tube
    const int m = juce::roundToInt (value (KngSat::mode));
    g.setFont (KngFonts::font (KngFonts::barlowHeavy, 18.0f, 0.1f));
    g.setColour (style.accent);
    g.drawText (KngSat::modeNames()[m].toUpperCase(), nameArea, juce::Justification::centred, false);
}

void KngSatView::paintCurve (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (area.getWidth() < 60.0f || area.getHeight() < 60.0f)
        return;

    const int m = juce::roundToInt (value (KngSat::mode));
    const float driveGain = juce::Decibels::decibelsToGain (value (KngSat::drive));

    auto toX = [&area] (float x) { return area.getCentreX() + x * area.getWidth() * 0.45f; };
    auto toY = [&area] (float y) { return area.getCentreY() - y * area.getHeight() * 0.45f; };

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
    g.drawVerticalLine ((int) area.getCentreX(), area.getY(), area.getBottom());

    // Clean, for comparison
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawLine (toX (-1.0f), toY (-1.0f), toX (1.0f), toY (1.0f), 1.0f);

    juce::Path curve;

    for (float x = -1.0f; x <= 1.0f; x += 0.01f)
    {
        const float y = juce::jlimit (-1.1f, 1.1f, KngSat::shape (m, x * driveGain));

        if (curve.isEmpty())
            curve.startNewSubPath (toX (x), toY (y));
        else
            curve.lineTo (toX (x), toY (y));
    }

    const auto hot = style.accent.interpolatedWith (juce::Colour (0xffffe6b0), glow * 0.5f);
    g.setColour (hot.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (hot);
    g.strokePath (curve, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (KngFonts::font (KngFonts::mono, 11.0f));
    g.setColour (style.label);
    g.drawText ("DRIVE " + juce::String (value (KngSat::drive), 1) + " dB", area.reduced (6.0f), juce::Justification::topLeft, false);
}
