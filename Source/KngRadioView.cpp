#include "KngRadioView.h"
#include "KngRadioPlugin.h"

namespace te = tracktion;

KngRadioView::KngRadioView (KngRadioPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xffd6a35c, 0xff4a3524, 0xff1c130c, 0xfffff4e0, 0xffc4a682, 0xffffd79a, 0xff21160d, 0xff4a3524,
                                 KngFonts::barlowSemi, true),
                   { juce::Colour (0xff0d0805), juce::Colour (0xff2e1e12), juce::Colour (0xff140d08), juce::Colour (0xff45301d) }),
      fx (owner)
{
    titleWidth = 200;
    sideWidth = 50;
    maxKnobWidth = 100;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    addHeaderChoice (*fx.getParam (KngRadioPlugin::band), 0.0f, KngRadioPlugin::bandNames().joinIntoString ("|"), 380);

    auto& toneKnob = addKnob (fx, KngRadioPlugin::tone, "Tone");
    toneKnob.formatter = [] (float v)
    {
        return std::abs (v) < 0.02f ? juce::String ("Center")
             : v > 0.0f ? "Up " + juce::String (juce::roundToInt (v * 100.0f)) + "%"
                        : "Down " + juce::String (juce::roundToInt (-v * 100.0f)) + "%";
    };

    addKnob (fx, KngRadioPlugin::drive, "Drive");
    addKnob (fx, KngRadioPlugin::crush, "Crush");
    addKnob (fx, KngRadioPlugin::noise, "Noise");
    addKnob (fx, KngRadioPlugin::mix, "Mix");
}

void KngRadioView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);
    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);

    const float level = juce::jlimit (0.0f, 1.0f, (outDb + 40.0f) / 40.0f);
    pulse += (level - pulse) * 0.35f;
}

void KngRadioView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "RADIO", KngFonts::barlowHeavy, 34.0f);
}

void KngRadioView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngRadioView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    auto inner = area.reduced (12.0f);
    auto grille = inner.removeFromLeft (juce::jmin (inner.getWidth() * 0.3f, inner.getHeight() * 1.1f));
    inner.removeFromLeft (14.0f);

    paintGrille (g, grille);
    paintDial (g, inner);
}

void KngRadioView::paintGrille (juce::Graphics& g, juce::Rectangle<float> a)
{
    const float d = juce::jmin (a.getWidth(), a.getHeight());
    const auto disc = juce::Rectangle<float> (d, d).withCentre (a.getCentre());
    const auto c = disc.getCentre();

    g.setColour (juce::Colour (0xff1a110a));
    g.fillEllipse (disc);
    g.setColour (style.accent.withAlpha (0.35f));
    g.drawEllipse (disc.reduced (1.0f), 2.0f);

    // Holes that light up from the middle as the sound gets louder
    const float spacing = d / 11.0f;
    const float radius = d * 0.5f - spacing * 0.6f;

    for (float y = -radius; y <= radius; y += spacing)
    {
        for (float x = -radius; x <= radius; x += spacing)
        {
            const float dist = std::sqrt (x * x + y * y);

            if (dist > radius)
                continue;

            const float lit = juce::jlimit (0.0f, 1.0f, pulse * 1.4f - dist / radius * 0.8f);
            const float r = spacing * (0.22f + 0.1f * lit);
            g.setColour (juce::Colour (0xff3a2a1c).interpolatedWith (style.accent, lit));
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre ({ c.x + x, c.y + y }));
        }
    }
}

void KngRadioView::paintDial (juce::Graphics& g, juce::Rectangle<float> a)
{
    if (a.getWidth() < 80.0f || a.getHeight() < 50.0f)
        return;

    auto nameArea = a.removeFromBottom (26.0f);
    const auto glass = a.withSizeKeepingCentre (a.getWidth(), juce::jmin (a.getHeight(), 90.0f));

    // Backlit dial glass
    juce::ColourGradient warm (juce::Colour (0xff3b2a17), glass.getCentreX(), glass.getCentreY(),
                               juce::Colour (0xff1a110a), glass.getX(), glass.getCentreY(), true);
    g.setGradientFill (warm);
    g.fillRoundedRectangle (glass, 8.0f);
    g.setColour (style.accent.withAlpha (0.4f));
    g.drawRoundedRectangle (glass.reduced (0.5f), 8.0f, 1.5f);

    const auto scale = glass.reduced (18.0f, 14.0f);
    auto xOf = [&scale] (float hz) { return scale.getX() + scale.getWidth() * juce::jlimit (0.0f, 1.0f, std::log (hz / 100.0f) / std::log (100.0f)); };

    // Tick marks and numbers, 100 Hz to 10 kHz
    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
    const float marks[] = { 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    const char* labels[] = { "100", "200", "500", "1k", "2k", "5k", "10k" };

    for (int k = 0; k < 7; ++k)
    {
        const float x = xOf (marks[k]);
        g.setColour (style.accent.withAlpha (0.6f));
        g.drawVerticalLine ((int) x, scale.getY(), scale.getY() + 10.0f);
        g.setColour (style.label);
        g.drawText (labels[k], juce::Rectangle<float> (x - 20.0f, scale.getBottom() - 12.0f, 40.0f, 12.0f), juce::Justification::centred, false);
    }

    for (float hz = 100.0f; hz <= 10000.0f; hz *= 1.122f)
    {
        g.setColour (style.accent.withAlpha (0.25f));
        g.drawVerticalLine ((int) xOf (hz), scale.getY(), scale.getY() + 5.0f);
    }

    // The band that gets through
    float lowHz = 0.0f, highHz = 0.0f;
    fx.getBandEdges (lowHz, highHz);
    const float x1 = xOf (lowHz), x2 = xOf (highHz);
    const auto lit = juce::Rectangle<float> (x1, scale.getY() + 14.0f, juce::jmax (2.0f, x2 - x1), scale.getHeight() - 30.0f);

    g.setColour (style.accent.withAlpha (0.15f + 0.25f * pulse));
    g.fillRoundedRectangle (lit, 4.0f);

    // The needle in the middle of the band
    const float nx = xOf (std::sqrt (lowHz * highHz));
    g.setColour (juce::Colour (0xffff5a36).withAlpha (0.35f));
    g.fillRect (nx - 3.0f, glass.getY() + 4.0f, 6.0f, glass.getHeight() - 8.0f);
    g.setColour (juce::Colour (0xffff5a36));
    g.fillRect (nx - 1.0f, glass.getY() + 4.0f, 2.0f, glass.getHeight() - 8.0f);

    // The sound's name
    const int b = juce::roundToInt (fx.getParam (KngRadioPlugin::band)->getCurrentValue());
    g.setFont (KngFonts::font (KngFonts::barlowHeavy, 18.0f, 0.15f));
    g.setColour (style.accent);
    g.drawText (KngRadioPlugin::bandNames()[b].toUpperCase(), nameArea, juce::Justification::centred, false);
}
