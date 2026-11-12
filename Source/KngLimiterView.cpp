#include "KngLimiterView.h"
#include "KngLimiterPlugin.h"

namespace te = tracktion;

KngLimiterView::KngLimiterView (KngLimiterPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xfff43f5e, 0xff47202a, 0xff18090d, 0xfffff0f2, 0xffd19aa6, 0xfffb8399, 0xff200b10, 0xff47202a,
                                 KngFonts::barlowSemi, true),
                   { juce::Colour (0xff0c0507), juce::Colour (0xff2a0f16), juce::Colour (0xff12060a), juce::Colour (0xff3e1822) }),
      fx (owner)
{
    titleWidth = 220;
    sideWidth = 50;
    maxKnobWidth = 120;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    addHeaderChoice (*fx.getParam (KngLimiterPlugin::style), 0.0f, "Transparent|Punchy|Loud", 320);

    addKnob (fx, KngLimiterPlugin::gain, "Gain");
    addKnob (fx, KngLimiterPlugin::ceiling, "Ceiling");
    addKnob (fx, KngLimiterPlugin::release, "Release");
}

void KngLimiterView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);

    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);
    grNow = gr > grNow ? gr : grNow * 0.85f;

    // The big readout holds the loudest peak for a moment
    const float peakDb = juce::Decibels::gainToDecibels (out, -100.0f);

    if (peakDb >= outHold)
    {
        outHold = peakDb;
        holdTicks = 45;
    }
    else if (--holdTicks <= 0)
    {
        outHold = juce::jmax (peakDb, outHold - 0.5f);
    }

    grHistory[(size_t) historyPos] = gr;
    outHistory[(size_t) historyPos] = peakDb;
    historyPos = (historyPos + 1) % (int) grHistory.size();
}

void KngLimiterView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "LIMITER", KngFonts::barlowHeavy, 36.0f);
}

void KngLimiterView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngLimiterView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    auto inner = area.reduced (12.0f);
    auto readout = inner.removeFromRight (juce::jmin (220.0f, inner.getWidth() * 0.3f));
    inner.removeFromRight (12.0f);
    const auto hist = inner;

    const float ceilingDb = fx.getParam (KngLimiterPlugin::ceiling)->getCurrentValue();
    auto toY = [&hist] (float db) { return hist.getY() + juce::jlimit (0.0f, 1.0f, -db / 24.0f) * hist.getHeight(); };

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (hist, 8.0f);

    {
        juce::Graphics::ScopedSaveState saveState (g);
        g.reduceClipRegion (hist.toNearestInt());

        const int n = (int) grHistory.size();
        const float colW = hist.getWidth() / (float) n;

        for (int k = 0; k < n; ++k)
        {
            const int idx = (historyPos + k) % n;
            const float x = hist.getX() + k * colW;

            // Output level, from the bottom
            const float outY = toY (outHistory[(size_t) idx]);
            g.setColour (style.accent.withAlpha (0.22f));
            g.fillRect (x, outY, colW + 1.0f, hist.getBottom() - outY);

            // Gain reduction, from the top (12 dB fills half)
            const float grH = juce::jlimit (0.0f, 1.0f, grHistory[(size_t) idx] / 24.0f) * hist.getHeight();
            g.setColour (style.accent.withAlpha (0.85f));
            g.fillRect (x, hist.getY(), colW + 1.0f, grH);
        }

        // The ceiling, never crossed
        const float cy = toY (ceilingDb);
        const float dash[] = { 6.0f, 4.0f };
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawDashedLine (juce::Line<float> (hist.getX(), cy, hist.getRight(), cy), dash, 2, 1.5f);
        g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
        g.drawText ("CEILING " + juce::String (ceilingDb, 1) + " dB",
                    juce::Rectangle<float> (hist.getX() + 8.0f, cy + 3.0f, 160.0f, 12.0f), juce::Justification::left, false);
    }

    // Big readouts
    g.setFont (KngFonts::font (KngFonts::barlowSemi, 13.0f, 0.2f));
    g.setColour (style.label);
    g.drawText ("OUTPUT PEAK", readout.removeFromTop (20.0f), juce::Justification::centredLeft, false);

    g.setFont (KngFonts::font (KngFonts::mono, juce::jmin (38.0f, readout.getHeight() * 0.3f)));
    g.setColour (outHold > ceilingDb - 0.2f ? style.accent : juce::Colours::white.withAlpha (0.92f));
    g.drawText (outHold > -60.0f ? juce::String (outHold, 1) : juce::String ("-inf"), readout.removeFromTop (48.0f),
                juce::Justification::centredLeft, false);

    readout.removeFromTop (8.0f);
    g.setFont (KngFonts::font (KngFonts::barlowSemi, 13.0f, 0.2f));
    g.setColour (style.label);
    g.drawText ("TAKEN OFF", readout.removeFromTop (20.0f), juce::Justification::centredLeft, false);

    g.setFont (KngFonts::font (KngFonts::mono, 22.0f));
    g.setColour (grNow > 0.05f ? style.accent : style.label);
    g.drawText ((grNow > 0.05f ? "-" : "") + juce::String (grNow, 1) + " dB", readout.removeFromTop (30.0f),
                juce::Justification::centredLeft, false);
}
