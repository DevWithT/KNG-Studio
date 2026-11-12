#include "KngCompView.h"
#include "KngCompPlugin.h"

namespace te = tracktion;

static constexpr float maxAngle = 0.85f;   // the needle's swing either side of straight up

static KngParamDef compDef (int p, const char* unit)
{
    const auto& in = KngComp::info (p);
    return { "", "", (float) in.min, (float) in.max, (float) in.def, (float) in.mid, (float) in.step, unit, nullptr };
}

//==============================================================================
KngCompView::KngCompView (KngCompPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xffffb020, 0xff4a3a22, 0xff1a140b, 0xfffff3dc, 0xffc8a978, 0xffffcf6a, 0xff1d160c, 0xff4a3a22,
                                 KngFonts::barlowSemi, true),
                   { juce::Colour (0xff0c0906), juce::Colour (0xff2a1f12), juce::Colour (0xff120d08), juce::Colour (0xff3a2c18) }),
      comp (owner)
{
    titleWidth = 170;
    sideWidth = 50;
    maxKnobWidth = 100;

    juce::StringArray names;

    for (auto& preset : KngComp::presets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { comp.applyPreset (i); });

    addHeaderChoice (*comp.getParam (KngComp::mode), (float) KngComp::info (KngComp::mode).min,
                     KngComp::modeNames().joinIntoString ("|"), 300);
    addHeaderSwitch (*comp.getParam (KngComp::autoMakeup), "Auto Gain",
                     "Turns the output back up by about what the compressor takes off", 130);

    addKnob (*comp.getParam (KngComp::threshold), compDef (KngComp::threshold, "dB"), "Threshold");

    auto& ratioKnob = addKnob (*comp.getParam (KngComp::ratio), compDef (KngComp::ratio, ""), "Ratio");
    ratioKnob.formatter = [] (float v) { return juce::String (v, v < 10.0f ? 1 : 0) + ":1"; };

    auto& attackKnob = addKnob (*comp.getParam (KngComp::attack), compDef (KngComp::attack, ""), "Attack");
    attackKnob.formatter = [] (float v) { return juce::String (v, v < 1.0f ? 2 : 1) + " ms"; };

    addKnob (*comp.getParam (KngComp::release), compDef (KngComp::release, "ms"), "Release");
    addKnob (*comp.getParam (KngComp::knee),    compDef (KngComp::knee, "dB"),    "Knee");
    addKnob (*comp.getParam (KngComp::makeup),  compDef (KngComp::makeup, "dB"),  "Makeup");
    addKnob (*comp.getParam (KngComp::mix),     compDef (KngComp::mix, "%"),      "Mix");
    addKnob (*comp.getParam (KngComp::scHpf),   compDef (KngComp::scHpf, "Hz"),   "SC Filter");
}

float KngCompView::value (int p) const
{
    return comp.getParam (p)->getCurrentValue();
}

void KngCompView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    comp.takeMeters (in, out, gr);

    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);
    grNow = gr > grNow ? gr : grNow * 0.9f;

    // The needle has weight: quick to move, gentle to settle
    needle += (grNow - needle) * 0.35f;
}

//==============================================================================
void KngCompView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "COMP", KngFonts::barlowHeavy, 38.0f);
}

void KngCompView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngCompView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    auto inner = area.reduced (12.0f);
    auto vu = inner.removeFromLeft (inner.getWidth() * 0.56f);
    inner.removeFromLeft (12.0f);

    paintVu (g, vu);
    paintTransfer (g, inner);
}

void KngCompView::paintVu (juce::Graphics& g, juce::Rectangle<float> a)
{
    if (a.getWidth() < 80.0f || a.getHeight() < 60.0f)
        return;

    const juce::Colour ink (0xff2a2015);

    // A warm, lit faceplate
    juce::ColourGradient plate (juce::Colour (0xfff6ead0), 0.0f, a.getY(), juce::Colour (0xffe2cfa6), 0.0f, a.getBottom(), false);
    g.setGradientFill (plate);
    g.fillRoundedRectangle (a, 10.0f);

    juce::ColourGradient warm (style.accent.withAlpha (0.22f), a.getCentreX(), a.getBottom(),
                               style.accent.withAlpha (0.0f), a.getCentreX(), a.getY(), true);
    g.setGradientFill (warm);
    g.fillRoundedRectangle (a, 10.0f);

    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (a.reduced (0.75f), 10.0f, 1.5f);

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (a.reduced (1.0f).toNearestInt());

    const juce::Point<float> pivot (a.getCentreX(), a.getBottom() + a.getHeight() * 0.18f);
    const float radius = juce::jmin (a.getWidth() * 0.46f, a.getHeight() * 0.95f);

    // 0 dB on the right, 20 dB of reduction on the left
    auto angleFor = [] (float gr)
    {
        const float frac = 1.0f - std::sqrt (juce::jlimit (0.0f, 1.0f, gr / 20.0f));
        return -maxAngle + frac * 2.0f * maxAngle;
    };

    auto pointAt = [&pivot] (float angle, float r)
    {
        return juce::Point<float> (pivot.x + std::sin (angle) * r, pivot.y - std::cos (angle) * r);
    };

    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, radius * 0.86f, radius * 0.86f, 0.0f, -maxAngle, maxAngle, true);
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.5f));

    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));

    for (float db : { 0.0f, 1.0f, 2.0f, 3.0f, 5.0f, 7.0f, 10.0f, 15.0f, 20.0f })
    {
        const float angle = angleFor (db);
        g.setColour (ink);
        g.drawLine (juce::Line<float> (pointAt (angle, radius * 0.86f), pointAt (angle, radius * 0.95f)), 1.5f);

        const auto label = juce::Rectangle<float> (30.0f, 14.0f).withCentre (pointAt (angle, radius * 1.05f));
        g.drawText (db == 0.0f ? juce::String ("0") : "-" + juce::String ((int) db), label, juce::Justification::centred, false);
    }

    g.setFont (KngFonts::font (KngFonts::barlowSemi, 12.0f, 0.2f));
    g.setColour (juce::Colour (0xff6b5434));
    g.drawText ("GAIN REDUCTION", juce::Rectangle<float> (a.getX(), a.getY() + a.getHeight() * 0.5f, a.getWidth(), 16.0f),
                juce::Justification::centred, false);

    g.setFont (KngFonts::font (KngFonts::mono, 16.0f));
    g.setColour (ink);
    g.drawText ((grNow > 0.05f ? "-" : "") + juce::String (grNow, 1) + " dB",
                juce::Rectangle<float> (a.getX(), a.getY() + a.getHeight() * 0.5f + 18.0f, a.getWidth(), 20.0f),
                juce::Justification::centred, false);

    // The needle, with a soft shadow
    const float angle = angleFor (needle);
    const auto tip = pointAt (angle, radius * 0.97f);
    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.drawLine (juce::Line<float> (pivot.translated (2.0f, 2.0f), tip.translated (2.0f, 2.0f)), 2.5f);
    g.setColour (juce::Colour (0xff1a1209));
    g.drawLine (juce::Line<float> (pivot, tip), 2.0f);
}

void KngCompView::paintTransfer (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (area.getWidth() < 60.0f || area.getHeight() < 60.0f)
        return;

    const float T = value (KngComp::threshold), R = value (KngComp::ratio), K = value (KngComp::knee);
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    const auto a = area.withSizeKeepingCentre (size, size);

    auto toX = [&a] (float db) { return a.getX() + (db + 60.0f) / 60.0f * a.getWidth(); };
    auto toY = [&a] (float db) { return a.getBottom() - (db + 60.0f) / 60.0f * a.getHeight(); };

    g.setColour (juce::Colours::white.withAlpha (0.05f));

    for (float db = -48.0f; db < 0.0f; db += 12.0f)
    {
        g.drawVerticalLine ((int) toX (db), a.getY(), a.getBottom());
        g.drawHorizontalLine ((int) toY (db), a.getX(), a.getRight());
    }

    // Straight line = no compression
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawLine (a.getX(), a.getBottom(), a.getRight(), a.getY(), 1.0f);

    // Threshold
    g.setColour (style.accent.withAlpha (0.5f));
    g.drawVerticalLine ((int) toX (T), a.getY(), a.getBottom());

    juce::Path curve;

    for (float db = -60.0f; db <= 0.0f; db += 0.5f)
    {
        const float y = toY (db + KngComp::gainComputer (db, T, R, K));

        if (curve.isEmpty())
            curve.startNewSubPath (toX (db), y);
        else
            curve.lineTo (toX (db), y);
    }

    g.setColour (style.accent.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xfffff3dc));
    g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Where the signal is right now
    if (inDb > -60.0f)
    {
        const float x = toX (inDb);
        const float y = toY (inDb + KngComp::gainComputer (inDb, T, R, K));
        g.setColour (style.accent.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre ({ x, y }));
        g.setColour (style.accent);
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ x, y }));
    }

    g.setColour (style.label);
    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
    g.drawText ("IN", a.reduced (6.0f), juce::Justification::bottomRight, false);
    g.drawText ("OUT", a.reduced (6.0f), juce::Justification::topLeft, false);
}
