#include "KngTransientView.h"
#include "KngTransientPlugin.h"

namespace te = tracktion;

KngTransientView::KngTransientView (KngTransientPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xff2dd4bf, 0xff1b3b38, 0xff091614, 0xffe6fffb, 0xff8fbcb6, 0xff99f6e4, 0xff0b1a18, 0xff1b3b38,
                                 KngFonts::spaceGrotesk, true),
                   { juce::Colour (0xff040a09), juce::Colour (0xff0e2220), juce::Colour (0xff071110), juce::Colour (0xff18352f) }),
      fx (owner)
{
    titleWidth = 230;
    sideWidth = 50;
    maxKnobWidth = 120;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    auto percent = [] (float v)
    {
        const int p = juce::roundToInt (v * 100.0f);
        return p == 0 ? juce::String ("0%") : (p > 0 ? "+" : "") + juce::String (p) + "%";
    };

    addKnob (fx, KngTransientPlugin::attack, "Attack").formatter = percent;
    addKnob (fx, KngTransientPlugin::sustain, "Sustain").formatter = percent;
    addKnob (fx, KngTransientPlugin::output, "Output");
    addKnob (fx, KngTransientPlugin::mix, "Mix");
}

void KngTransientView::tick()
{
    float in = 0.0f, out = 0.0f, move = 0.0f;
    fx.takeMeters (in, out, move);
    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);

    // The plugin sends its level move offset by +24 so it's never negative; nothing sent reads as no move
    moveHistory[(size_t) historyPos] = move > 0.0f ? move - 24.0f : 0.0f;
    historyPos = (historyPos + 1) % (int) moveHistory.size();
}

void KngTransientView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "TRANSIENT", KngFonts::spaceGrotesk, 26.0f);
}

void KngTransientView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngTransientView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    auto inner = area.reduced (14.0f, 12.0f);
    auto readout = inner.removeFromTop (18.0f);
    auto history = inner.removeFromBottom (inner.getHeight() * 0.32f);
    inner.removeFromBottom (8.0f);
    const auto hitArea = inner;

    // A sample hit: how it looks now (faint) and after this effect (bright)
    juce::Path before, after;

    for (int k = 0; k <= 160; ++k)
    {
        const float t = (float) k / 160.0f;
        const float env = t < 0.02f ? t / 0.02f : std::exp (-(t - 0.02f) * 6.0f);
        const float shaped = juce::jlimit (0.0f, 1.6f, env * juce::Decibels::decibelsToGain (fx.exampleGainDb (t)));
        const float x = hitArea.getX() + t * hitArea.getWidth();
        const float y0 = hitArea.getBottom() - env / 1.6f * hitArea.getHeight();
        const float y1 = hitArea.getBottom() - shaped / 1.6f * hitArea.getHeight();

        if (k == 0)
        {
            before.startNewSubPath (x, y0);
            after.startNewSubPath (x, y1);
        }
        else
        {
            before.lineTo (x, y0);
            after.lineTo (x, y1);
        }
    }

    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.strokePath (before, juce::PathStrokeType (1.5f));

    auto fill = after;
    fill.lineTo (hitArea.getRight(), hitArea.getBottom());
    fill.lineTo (hitArea.getX(), hitArea.getBottom());
    fill.closeSubPath();
    g.setColour (style.accent.withAlpha (0.18f));
    g.fillPath (fill);
    g.setColour (style.accent);
    g.strokePath (after, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Live: how much it's pushing (up) or pulling (down) the level
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (history, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine ((int) history.getCentreY(), history.getX(), history.getRight());

    const int n = (int) moveHistory.size();
    const float colW = history.getWidth() / (float) n;

    for (int k = 0; k < n; ++k)
    {
        const float db = moveHistory[(size_t) ((historyPos + k) % n)];
        const float h = juce::jlimit (0.0f, 1.0f, std::abs (db) / 12.0f) * history.getHeight() * 0.5f;
        const float x = history.getX() + k * colW;

        g.setColour (db >= 0.0f ? style.accent.withAlpha (0.8f) : juce::Colour (0xffff7a7a).withAlpha (0.8f));

        if (db >= 0.0f)
            g.fillRect (x, history.getCentreY() - h, colW + 0.5f, h);
        else
            g.fillRect (x, history.getCentreY(), colW + 0.5f, h);
    }

    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    const int a = juce::roundToInt (fx.getParam (KngTransientPlugin::attack)->getCurrentValue() * 100.0f);
    const int s = juce::roundToInt (fx.getParam (KngTransientPlugin::sustain)->getCurrentValue() * 100.0f);
    g.drawText ("ATTACK " + juce::String (a) + "%   SUSTAIN " + juce::String (s) + "%", readout, juce::Justification::centredLeft, false);
}
