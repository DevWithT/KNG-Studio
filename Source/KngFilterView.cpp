#include "KngFilterView.h"
#include "KngFilterPlugin.h"

namespace te = tracktion;

KngFilterView::KngFilterView (KngFilterPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xfff472b6, 0xff4a2338, 0xff180b12, 0xfffff0f7, 0xffd39ab8, 0xfffbcfe8, 0xff1d0c16, 0xff4a2338,
                                 KngFonts::spaceGrotesk, false),
                   { juce::Colour (0xff0c0509), juce::Colour (0xff2a1020), juce::Colour (0xff12070d), juce::Colour (0xff3d1a2e) }),
      fx (owner)
{
    titleWidth = 180;
    sideWidth = 50;
    maxKnobWidth = 100;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    addHeaderChoice (*fx.getParam (KngFilterPlugin::mode), 0.0f, "Low Pass|High Pass|Band Pass", 330);
    addHeaderChoice (*fx.getParam (KngFilterPlugin::lfoRate), 0.0f, "Off|2 Bars|1 Bar|1/2|1/4|1/8", 380);

    auto& cutKnob = addKnob (fx, KngFilterPlugin::cutoff, "Cutoff");
    cutKnob.formatter = [] (float v) { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; };

    addKnob (fx, KngFilterPlugin::reso, "Reso");
    addKnob (fx, KngFilterPlugin::drive, "Drive");
    addKnob (fx, KngFilterPlugin::lfoDepth, "Depth");
    addKnob (fx, KngFilterPlugin::mix, "Mix");
}

void KngFilterView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);
    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);
}

void KngFilterView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "Filter", KngFonts::spaceGrotesk, 30.0f);
}

void KngFilterView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngFilterView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (area.reduced (1.0f).toNearestInt());

    const auto plot = area.reduced (16.0f, 14.0f).withTrimmedTop (14.0f);
    auto xOf = [&plot] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f); };
    auto yOf = [&plot] (float db) { return plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, (24.0f - db) / 60.0f); };

    const float gridHz[] = { 100.0f, 1000.0f, 10000.0f };
    const char* gridText[] = { "100", "1k", "10k" };
    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));

    for (int k = 0; k < 3; ++k)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) xOf (gridHz[k]), plot.getY(), plot.getBottom());
        g.setColour (style.label.withAlpha (0.7f));
        g.drawText (gridText[k], juce::Rectangle<float> (xOf (gridHz[k]) + 3.0f, plot.getBottom() - 13.0f, 30.0f, 12.0f),
                    juce::Justification::left, false);
    }

    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine ((int) yOf (0.0f), plot.getX(), plot.getRight());

    const float set = fx.getParam (KngFilterPlugin::cutoff)->getCurrentValue();
    const float live = fx.getLiveCutoff();

    auto curveFor = [&] (float cutoffHz)
    {
        juce::Path p;

        for (int k = 0; k <= 140; ++k)
        {
            const float hz = 20.0f * std::pow (1000.0f, (float) k / 140.0f);
            const float x = xOf (hz), y = yOf (fx.responseAt (hz, cutoffHz));

            if (k == 0)
                p.startNewSubPath (x, y);
            else
                p.lineTo (x, y);
        }

        return p;
    };

    // Where the knob sits (faint), and where the wobble has it right now (bright)
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.strokePath (curveFor (set), juce::PathStrokeType (1.2f));

    const auto curve = curveFor (live);
    auto fill = curve;
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();

    juce::ColourGradient grad (style.accent.withAlpha (0.3f), 0.0f, plot.getY(), style.accent.withAlpha (0.0f), 0.0f, plot.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (fill);
    g.setColour (style.accent.withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (style.accent);
    g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // A dot on the cutoff
    const juce::Point<float> dot (xOf (live), yOf (fx.responseAt (live, live)));
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (dot));

    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText ("CUTOFF " + (live >= 1000.0f ? juce::String (live / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (live)) + " Hz"),
                area.reduced (14.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, false);
}
