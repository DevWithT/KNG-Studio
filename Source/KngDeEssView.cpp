#include "KngDeEssView.h"
#include "KngDeEssPlugin.h"

namespace te = tracktion;

KngDeEssView::KngDeEssView (KngDeEssPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xffe879f9, 0xff3e2445, 0xff160b19, 0xfffdf0ff, 0xffc79ad3, 0xfff0abfc, 0xff1c0d20, 0xff3e2445,
                                 KngFonts::sora, false),
                   { juce::Colour (0xff0a050c), juce::Colour (0xff241028), juce::Colour (0xff100612), juce::Colour (0xff341a3a) }),
      fx (owner)
{
    titleWidth = 210;
    sideWidth = 50;
    maxKnobWidth = 120;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    addHeaderChoice (*fx.getParam (KngDeEssPlugin::mode), 0.0f, "Split|Wide", 160);
    addHeaderSwitch (*fx.getParam (KngDeEssPlugin::listen), "Listen", "Hear only what it catches, to tune Frequency", 110);

    addKnob (fx, KngDeEssPlugin::freq, "Frequency");
    addKnob (fx, KngDeEssPlugin::threshold, "Threshold");
    addKnob (fx, KngDeEssPlugin::range, "Range");
}

void KngDeEssView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);

    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);
    grNow = gr > grNow ? gr : grNow * 0.85f;

    grHistory[(size_t) historyPos] = gr;
    inHistory[(size_t) historyPos] = juce::Decibels::gainToDecibels (in, -100.0f);
    historyPos = (historyPos + 1) % (int) grHistory.size();
}

void KngDeEssView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "De-Esser", KngFonts::sora, 26.0f);
}

void KngDeEssView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngDeEssView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    auto inner = area.reduced (12.0f);
    auto strip = inner.removeFromTop (juce::jmax (60.0f, inner.getHeight() * 0.42f));
    inner.removeFromTop (10.0f);
    const auto hist = inner;

    const float f = fx.getParam (KngDeEssPlugin::freq)->getCurrentValue();
    const float rangeDb = juce::jmax (1.0f, fx.getParam (KngDeEssPlugin::range)->getCurrentValue());
    const float caught = juce::jlimit (0.0f, 1.0f, grNow / rangeDb);

    // Frequency strip, 500 Hz to 20 kHz
    auto toX = [&strip] (float hz) { return strip.getX() + strip.getWidth() * std::log (hz / 500.0f) / std::log (40.0f); };

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (strip, 8.0f);

    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));

    for (float hz : { 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = toX (hz);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawVerticalLine ((int) x, strip.getY(), strip.getBottom());
        g.setColour (style.label);
        g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x + 3.0f, strip.getBottom() - 14.0f, 30.0f, 12.0f), juce::Justification::left, false);
    }

    // Everything above the frequency is what it listens to
    const float fx1 = toX (f);
    const auto region = juce::Rectangle<float>::leftTopRightBottom (fx1, strip.getY(), strip.getRight(), strip.getBottom());
    juce::ColourGradient lit (style.accent.withAlpha (0.2f + 0.35f * caught), fx1, 0.0f, style.accent.withAlpha (0.05f), strip.getRight(), 0.0f, false);
    g.setGradientFill (lit);
    g.fillRoundedRectangle (region, 8.0f);
    g.setColour (style.accent);
    g.fillRect (fx1 - 1.0f, strip.getY(), 2.0f, strip.getHeight());

    g.setFont (KngFonts::font (KngFonts::mono, 11.0f));
    g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz",
                juce::Rectangle<float> (fx1 + 6.0f, strip.getY() + 6.0f, 80.0f, 14.0f), juce::Justification::left, false);

    // The S lights up when it's catching one
    g.setFont (KngFonts::font (KngFonts::sora, juce::jmin (44.0f, strip.getHeight() * 0.8f)));
    g.setColour (style.accent.withAlpha (0.25f + 0.75f * caught));
    g.drawText ("S", region, juce::Justification::centred, false);

    // Reduction history (from the top) over the input level (from the bottom)
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

            const float inH = juce::jlimit (0.0f, 1.0f, (inHistory[(size_t) idx] + 60.0f) / 60.0f) * hist.getHeight();
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRect (x, hist.getBottom() - inH, colW + 1.0f, inH);

            const float grH = juce::jlimit (0.0f, 1.0f, grHistory[(size_t) idx] / 24.0f) * hist.getHeight();
            g.setColour (style.accent.withAlpha (0.7f));
            g.fillRect (x, hist.getY(), colW + 1.0f, grH);
        }
    }

    g.setFont (KngFonts::font (KngFonts::sora, 22.0f));
    g.setColour (grNow > 0.05f ? style.accent : style.label);
    g.drawText ((grNow > 0.05f ? "-" : "") + juce::String (grNow, 1) + " dB", hist.reduced (10.0f, 6.0f), juce::Justification::topRight, false);
    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
    g.setColour (style.label);
    g.drawText ("TAKEN OFF", hist.reduced (10.0f, 6.0f).withTrimmedTop (28.0f), juce::Justification::topRight, false);
}
