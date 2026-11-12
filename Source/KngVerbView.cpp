#include "KngVerbView.h"
#include "KngVerbPlugin.h"

namespace te = tracktion;

static KngParamDef verbDef (int p, const char* unit)
{
    const auto& in = KngVerb::info (p);
    return { "", "", (float) in.min, (float) in.max, (float) in.def, (float) in.mid, (float) in.step, unit, nullptr };
}

static juce::String hzText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
}

//==============================================================================
KngVerbView::KngVerbView (KngVerbPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xff5b8cff, 0xff26335a, 0xff0c1124, 0xffeef2ff, 0xff8e9fd0, 0xffa8c0ff, 0xff0d1430, 0xff22305a,
                                 KngFonts::sora, false),
                   { juce::Colour (0xff060912), juce::Colour (0xff121a38), juce::Colour (0xff080c1c), juce::Colour (0xff1e2a52) }),
      verb (owner)
{
    titleWidth = 190;
    sideWidth = 22;
    maxKnobWidth = 86;

    juce::StringArray names;

    for (auto& preset : KngVerb::presets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { verb.applyPreset (i); });

    using namespace KngVerb;

    addKnob (*verb.getParam (preDelay), verbDef (preDelay, "ms"), "Pre-Delay");

    auto& decayKnob = addKnob (*verb.getParam (decay), verbDef (decay, ""), "Decay");
    decayKnob.formatter = [] (float v) { return juce::String (v, v < 10.0f ? 2 : 1) + " s"; };

    addKnob (*verb.getParam (size),       verbDef (size, "%"),       "Size");
    addKnob (*verb.getParam (diffusion),  verbDef (diffusion, "%"),  "Diffusion");
    addKnob (*verb.getParam (damping),    verbDef (damping, "Hz"),   "Damping");
    addKnob (*verb.getParam (modulation), verbDef (modulation, "%"), "Motion");
    addKnob (*verb.getParam (width),      verbDef (width, "%"),      "Width");
    addKnob (*verb.getParam (duck),       verbDef (duck, "%"),       "Duck");
    addKnob (*verb.getParam (lowCut),     verbDef (lowCut, "Hz"),    "Low Cut");
    addKnob (*verb.getParam (highCut),    verbDef (highCut, "Hz"),   "High Cut");
    addKnob (*verb.getParam (mix),        verbDef (mix, "%"),        "Mix");
}

float KngVerbView::value (int p) const
{
    return verb.getParam (p)->getCurrentValue();
}

void KngVerbView::tick()
{
    wetDb = fallingDb (verb.takeWetPeak(), wetDb);

    // The rings drift outward, faster for a small room
    phase += 0.004f + 0.006f * (1.0f - value (KngVerb::size));

    if (phase >= 1.0f)
        phase -= 1.0f;
}

//==============================================================================
void KngVerbView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "Verb", KngFonts::sora, 28.0f);
}

void KngVerbView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawMeter (g, area, wetDb, "WET");
}

void KngVerbView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace KngVerb;

    drawScreen (g, area);

    auto inner = area.reduced (10.0f);
    const auto band = inner.removeFromBottom (20.0f);
    inner.removeFromBottom (8.0f);

    const float sizeAmt = value (size), decaySec = value (decay), pre = value (preDelay);
    const float level = juce::jlimit (0.0f, 1.0f, (wetDb + 50.0f) / 50.0f);
    const float darkness = juce::jlimit (0.0f, 1.0f, 1.0f - (value (damping) - 1000.0f) / 19000.0f);
    const auto ringColour = style.accent.interpolatedWith (juce::Colour (0xff8b5cf6), darkness * 0.8f);

    {
        juce::Graphics::ScopedSaveState saveState (g);
        g.reduceClipRegion (inner.toNearestInt());

        // The room: Size sets how far the rings reach, Pre-Delay leaves a gap around the source,
        // Decay sets how slowly they fade, and the reverb's level lights them up
        const auto c = inner.getCentre();
        const float maxRx = inner.getWidth() * 0.48f * (0.45f + 0.55f * sizeAmt);
        const float maxRy = inner.getHeight() * 0.46f * (0.45f + 0.55f * sizeAmt);
        const float gap = juce::jlimit (0.06f, 0.46f, 0.06f + pre / 300.0f * 0.4f);
        const float fadePower = 1.5f / juce::jlimit (0.2f, 4.0f, decaySec / 2.0f);
        constexpr int rings = 8;

        for (int k = 0; k < rings; ++k)
        {
            const float t = std::fmod ((float) k / (float) rings + phase, 1.0f);
            const float rr = gap + (1.0f - gap) * t;
            const float alpha = std::pow (1.0f - t, fadePower) * (0.25f + 0.75f * level);

            g.setColour (ringColour.withAlpha (juce::jlimit (0.0f, 1.0f, alpha)));
            g.drawEllipse (juce::Rectangle<float> (maxRx * 2.0f * rr, maxRy * 2.0f * rr).withCentre (c), 1.5f + 2.0f * (1.0f - t));
        }

        // The source
        const float glowR = 26.0f + 20.0f * level;
        juce::ColourGradient glow (ringColour.withAlpha (0.55f), c.x, c.y, ringColour.withAlpha (0.0f), c.x + glowR, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (juce::Rectangle<float> (glowR * 2.0f, glowR * 2.0f).withCentre (c));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (c));

        // Readouts
        const auto text = inner.reduced (12.0f, 8.0f);
        g.setFont (KngFonts::font (KngFonts::sora, 26.0f));
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.drawText (juce::String (decaySec, decaySec < 10.0f ? 2 : 1) + " s", text, juce::Justification::topRight, false);
        g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
        g.setColour (style.label);
        g.drawText ("DECAY", text.withTrimmedTop (32.0f), juce::Justification::topRight, false);
        g.drawText ("SIZE " + juce::String (juce::roundToInt (sizeAmt * 100.0f)) + "%   PRE " + juce::String (juce::roundToInt (pre)) + " ms",
                    text, juce::Justification::topLeft, false);
    }

    // The range the reverb covers (Low Cut to High Cut)
    auto toX = [&band] (float f) { return band.getX() + band.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
    const float x1 = toX (juce::jmax (20.0f, value (lowCut)));
    const float x2 = toX (juce::jmax (20.0f, value (highCut)));

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (band, 4.0f);
    g.setColour (ringColour.withAlpha (0.45f));
    g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (x1, band.getY() + 3.0f, juce::jmax (x1 + 2.0f, x2), band.getBottom() - 3.0f), 3.0f);
    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
    g.setColour (juce::Colours::white.withAlpha (0.75f));
    g.drawText ("REVERB RANGE  " + hzText (value (lowCut)) + " - " + hzText (value (highCut)), band.reduced (8.0f, 0.0f),
                juce::Justification::centredLeft, false);
}
