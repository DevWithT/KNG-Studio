#include "KngGateView.h"
#include "KngGatePlugin.h"

namespace te = tracktion;

KngGateView::KngGateView (KngGatePlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xffa3e635, 0xff2c3a1c, 0xff10160a, 0xfff5ffe6, 0xffa7b98f, 0xffd9f99d, 0xff131a0c, 0xff2c3a1c,
                                 KngFonts::barlowSemi, true),
                   { juce::Colour (0xff070a04), juce::Colour (0xff18220e), juce::Colour (0xff0c1107), juce::Colour (0xff283618) }),
      fx (owner)
{
    titleWidth = 190;
    sideWidth = 50;
    maxKnobWidth = 100;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    addHeaderChoice (*fx.getParam (KngGatePlugin::mode), 0.0f, "Gate|Soft", 160);

    addKnob (fx, KngGatePlugin::threshold, "Threshold");
    addKnob (fx, KngGatePlugin::range, "Range");
    addKnob (fx, KngGatePlugin::attack, "Attack");
    addKnob (fx, KngGatePlugin::hold, "Hold");
    addKnob (fx, KngGatePlugin::release, "Release");
    addKnob (fx, KngGatePlugin::lowCut, "Low Cut");
}

void KngGateView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);
    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);

    inHistory[(size_t) historyPos] = juce::Decibels::gainToDecibels (in, -100.0f);
    grHistory[(size_t) historyPos] = gr;
    historyPos = (historyPos + 1) % (int) inHistory.size();
}

void KngGateView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "GATE", KngFonts::barlowHeavy, 36.0f);
}

void KngGateView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngGateView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    auto inner = area.reduced (12.0f);
    auto lampArea = inner.removeFromRight (juce::jmin (120.0f, inner.getWidth() * 0.22f));
    inner.removeFromRight (12.0f);
    auto strip = inner.removeFromBottom (10.0f);
    inner.removeFromBottom (6.0f);
    const auto hist = inner;

    const float thr = fx.getParam (KngGatePlugin::threshold)->getCurrentValue();
    auto toY = [&hist] (float db) { return hist.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 80.0f) * hist.getHeight(); };

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (hist, 8.0f);

    {
        juce::Graphics::ScopedSaveState saveState (g);
        g.reduceClipRegion (hist.toNearestInt());

        const int n = (int) inHistory.size();
        const float colW = hist.getWidth() / (float) n;

        for (int k = 0; k < n; ++k)
        {
            const int idx = (historyPos + k) % n;
            const float x = hist.getX() + k * colW;
            const float y = toY (inHistory[(size_t) idx]);
            const bool shut = grHistory[(size_t) idx] > 3.0f;

            // Above the line: let through (bright). Below: held back (dim).
            g.setColour (style.accent.withAlpha (shut ? 0.14f : 0.55f));
            g.fillRect (x, y, colW + 1.0f, hist.getBottom() - y);
        }

        const float ty = toY (thr);
        const float dash[] = { 6.0f, 4.0f };
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawDashedLine (juce::Line<float> (hist.getX(), ty, hist.getRight(), ty), dash, 2, 1.5f);
        g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
        g.drawText ("THRESHOLD " + juce::String (thr, 1) + " dB",
                    juce::Rectangle<float> (hist.getX() + 8.0f, ty - 15.0f, 180.0f, 12.0f), juce::Justification::left, false);
    }

    // Open / closed strip under the history
    {
        const int n = (int) grHistory.size();
        const float colW = strip.getWidth() / (float) n;

        for (int k = 0; k < n; ++k)
        {
            const int idx = (historyPos + k) % n;
            const bool shut = grHistory[(size_t) idx] > 3.0f;
            g.setColour (shut ? juce::Colours::white.withAlpha (0.06f) : style.accent.withAlpha (0.8f));
            g.fillRect (strip.getX() + k * colW, strip.getY(), colW + 1.0f, strip.getHeight());
        }
    }

    // The lamp
    const bool isOpen = fx.isOpen();
    const float d = juce::jmin (lampArea.getWidth() * 0.6f, lampArea.getHeight() * 0.5f);
    const auto lamp = juce::Rectangle<float> (d, d).withCentre ({ lampArea.getCentreX(), lampArea.getCentreY() - 10.0f });

    if (isOpen)
    {
        juce::ColourGradient glow (style.accent.withAlpha (0.55f), lamp.getCentreX(), lamp.getCentreY(),
                                   style.accent.withAlpha (0.0f), lamp.getCentreX() + d, lamp.getCentreY(), true);
        g.setGradientFill (glow);
        g.fillEllipse (lamp.expanded (d * 0.5f));
    }

    g.setColour (isOpen ? style.accent : juce::Colour (0xff2a3320));
    g.fillEllipse (lamp);
    g.setColour (juce::Colours::white.withAlpha (isOpen ? 0.5f : 0.12f));
    g.drawEllipse (lamp, 1.5f);

    g.setFont (KngFonts::font (KngFonts::barlowHeavy, 18.0f, 0.15f));
    g.setColour (isOpen ? style.accent : style.label);
    g.drawText (isOpen ? "OPEN" : "CLOSED", juce::Rectangle<float> (lampArea.getX(), lamp.getBottom() + 6.0f, lampArea.getWidth(), 22.0f),
                juce::Justification::centred, false);
}
