#include "KngWidthView.h"
#include "KngWidthPlugin.h"

namespace te = tracktion;

KngWidthView::KngWidthView (KngWidthPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xff818cf8, 0xff272a4a, 0xff0d0e1a, 0xffeef0ff, 0xff9ea3d6, 0xffc7cbff, 0xff10111f, 0xff272a4a,
                                 KngFonts::spaceGrotesk, false),
                   { juce::Colour (0xff06070d), juce::Colour (0xff171a33), juce::Colour (0xff0b0c17), juce::Colour (0xff24284a) }),
      fx (owner)
{
    titleWidth = 180;
    sideWidth = 50;
    maxKnobWidth = 120;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    addKnob (fx, KngWidthPlugin::width, "Width");

    auto& bassKnob = addKnob (fx, KngWidthPlugin::monoBass, "Mono Bass");
    bassKnob.formatter = [] (float v) { return v <= 21.0f ? juce::String ("Off") : juce::String (juce::roundToInt (v)) + " Hz"; };

    auto& haasKnob = addKnob (fx, KngWidthPlugin::haas, "Haas");
    haasKnob.formatter = [] (float v) { return v < 0.1f ? juce::String ("Off") : juce::String (v, 1) + " ms"; };

    addKnob (fx, KngWidthPlugin::output, "Output");
}

void KngWidthView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);
    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);
}

void KngWidthView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "Width", KngFonts::spaceGrotesk, 30.0f);
}

void KngWidthView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngWidthView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    auto inner = area.reduced (14.0f, 12.0f);
    auto corrArea = inner.removeFromBottom (26.0f);
    inner.removeFromBottom (6.0f);

    // The scope: a straight up-and-down line is mono, a wide cloud is wide
    const float size = juce::jmin (inner.getWidth(), inner.getHeight());
    const auto scope = juce::Rectangle<float> (size, size).withCentre (inner.getCentre());
    const float cx = scope.getCentreX(), cy = scope.getCentreY(), half = size * 0.5f;

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (scope.reduced (1.0f), 1.0f);
    g.drawLine (cx, scope.getY(), cx, scope.getBottom(), 1.0f);
    g.drawLine (scope.getX(), cy, scope.getRight(), cy, 1.0f);

    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
    g.setColour (style.label.withAlpha (0.7f));
    g.drawText ("L", juce::Rectangle<float> (scope.getX() + 6.0f, scope.getY() + 6.0f, 14.0f, 12.0f), juce::Justification::centred, false);
    g.drawText ("R", juce::Rectangle<float> (scope.getRight() - 20.0f, scope.getY() + 6.0f, 14.0f, 12.0f), juce::Justification::centred, false);

    {
        juce::Graphics::ScopedSaveState saveState (g);
        g.reduceClipRegion (scope.toNearestInt());

        const int pos = fx.getScopePos();
        constexpr int n = KngWidthPlugin::scopeSize;

        // Loudest recent sample, so quiet sounds still fill the scope
        float peak = 0.05f;

        for (int k = 0; k < n; ++k)
            peak = juce::jmax (peak, std::abs (fx.getScopeL (k)), std::abs (fx.getScopeR (k)));

        const float scale = half * 0.85f / peak;

        for (int k = 0; k < n; ++k)
        {
            const int idx = (pos + k) % n;
            const float l = fx.getScopeL (idx), r = fx.getScopeR (idx);
            const float x = cx + (r - l) * 0.7071f * scale;
            const float y = cy - (l + r) * 0.7071f * scale;
            const float age = (float) k / (float) n;

            g.setColour (style.accent.withAlpha (0.1f + 0.6f * age));
            g.fillRect (x - 1.0f, y - 1.0f, 2.0f, 2.0f);
        }
    }

    // Mono safety: right is mono-safe, left of centre can disappear on phones and club systems
    const float corr = juce::jlimit (-1.0f, 1.0f, fx.getCorrelation());
    auto bar = corrArea.withSizeKeepingCentre (juce::jmin (corrArea.getWidth(), 360.0f), 8.0f);

    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (bar, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawVerticalLine ((int) bar.getCentreX(), bar.getY() - 3.0f, bar.getBottom() + 3.0f);

    const float mx = bar.getCentreX() + corr * bar.getWidth() * 0.5f;
    g.setColour (corr < 0.0f ? juce::Colour (0xffff5d5d) : style.accent);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ mx, bar.getCentreY() }));

    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
    g.setColour (style.label);
    g.drawText ("-1", bar.withWidth (20.0f).translated (-24.0f, 0.0f).expanded (0.0f, 4.0f), juce::Justification::centredRight, false);
    g.drawText ("+1", bar.withX (bar.getRight() + 4.0f).withWidth (20.0f).expanded (0.0f, 4.0f), juce::Justification::centredLeft, false);

    // Readout
    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText ("WIDTH " + juce::String (juce::roundToInt (fx.getParam (KngWidthPlugin::width)->getCurrentValue() * 100.0f)) + "%",
                area.reduced (14.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, false);
}
