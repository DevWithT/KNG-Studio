#include "KngLoFiView.h"
#include "KngLoFiPlugin.h"

namespace te = tracktion;

KngLoFiView::KngLoFiView (KngLoFiPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xfff97316, 0xff4a2c18, 0xff1a0f08, 0xfffff1e6, 0xffcfa98a, 0xfffdba74, 0xff1f1109, 0xff4a2c18,
                                 KngFonts::barlowSemi, true),
                   { juce::Colour (0xff0c0704), juce::Colour (0xff2b170b), juce::Colour (0xff130b05), juce::Colour (0xff42260f) }),
      fx (owner)
{
    titleWidth = 190;
    sideWidth = 50;
    maxKnobWidth = 100;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    auto& bitsKnob = addKnob (fx, KngLoFiPlugin::bits, "Bits");
    bitsKnob.formatter = [] (float v) { return juce::String (v, 1) + " bit"; };

    auto& rateKnob = addKnob (fx, KngLoFiPlugin::rate, "Rate");
    rateKnob.formatter = [] (float v) { return juce::String (v / 1000.0f, 1) + " kHz"; };

    addKnob (fx, KngLoFiPlugin::wobble, "Wobble");
    addKnob (fx, KngLoFiPlugin::dust, "Dust");

    auto& toneKnob = addKnob (fx, KngLoFiPlugin::tone, "Tone");
    toneKnob.formatter = [] (float v) { return juce::String (v / 1000.0f, 1) + " kHz"; };

    addKnob (fx, KngLoFiPlugin::mix, "Mix");
}

float KngLoFiView::value (int p) const
{
    return fx.getParam (p)->getCurrentValue();
}

void KngLoFiView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);
    inDb = fallingDb (in, inDb);
    outDb = fallingDb (out, outDb);

    // The reels turn while sound comes through, and wobble with Wobble
    clock += 1.0 / 30.0;
    const bool running = outDb > -60.0f;
    const float wobble = value (KngLoFiPlugin::wobble);
    const float speed = running ? 0.12f * (1.0f + wobble * 0.35f * (float) std::sin (clock * 3.8)) : 0.0f;
    angle += speed;
}

void KngLoFiView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "LO-FI", KngFonts::barlowHeavy, 34.0f);
}

void KngLoFiView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, inDb, outDb);
}

void KngLoFiView::paintReel (juce::Graphics& g, juce::Point<float> c, float r, float a, float tapeAmount)
{
    // Tape wound on the reel
    const float tapeR = r * (0.45f + 0.5f * tapeAmount);
    g.setColour (juce::Colour (0xff2a1a10));
    g.fillEllipse (juce::Rectangle<float> (tapeR * 2.0f, tapeR * 2.0f).withCentre (c));

    // Hub with three spokes
    const float hubR = r * 0.4f;
    g.setColour (juce::Colour (0xfff5e6d3));
    g.fillEllipse (juce::Rectangle<float> (hubR * 2.0f, hubR * 2.0f).withCentre (c));
    g.setColour (juce::Colour (0xff1a0f08));
    g.fillEllipse (juce::Rectangle<float> (hubR * 0.7f, hubR * 0.7f).withCentre (c));

    for (int k = 0; k < 3; ++k)
    {
        const float t = a + (float) k * juce::MathConstants<float>::twoPi / 3.0f;
        const juce::Point<float> tip (c.x + std::cos (t) * hubR * 0.95f, c.y + std::sin (t) * hubR * 0.95f);
        g.drawLine (juce::Line<float> (c, tip), hubR * 0.28f);
    }
}

void KngLoFiView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    auto inner = area.reduced (14.0f, 12.0f);
    auto readout = inner.removeFromTop (18.0f);

    // The cassette shell
    const float w = juce::jmin (inner.getWidth(), inner.getHeight() * 1.7f);
    const auto shell = juce::Rectangle<float> (w, w / 1.7f).withCentre (inner.getCentre());

    g.setColour (juce::Colour (0xff3a2617));
    g.fillRoundedRectangle (shell, 10.0f);
    g.setColour (style.accent.withAlpha (0.5f));
    g.drawRoundedRectangle (shell.reduced (1.0f), 10.0f, 1.5f);

    // Label strip
    auto label = shell.reduced (shell.getWidth() * 0.06f, shell.getHeight() * 0.08f).removeFromTop (shell.getHeight() * 0.2f);
    g.setColour (style.accent);
    g.fillRoundedRectangle (label, 4.0f);
    g.setColour (juce::Colour (0xff1a0f08));
    g.setFont (KngFonts::font (KngFonts::barlowHeavy, label.getHeight() * 0.7f, 0.1f));
    g.drawText ("KNG  C-60", label.reduced (8.0f, 0.0f), juce::Justification::centredLeft, false);

    // Window with the two reels
    const auto window = shell.withSizeKeepingCentre (shell.getWidth() * 0.62f, shell.getHeight() * 0.38f).translated (0.0f, shell.getHeight() * 0.06f);
    g.setColour (juce::Colour (0xff120a05));
    g.fillRoundedRectangle (window, window.getHeight() * 0.5f);

    const float r = window.getHeight() * 0.48f;
    const juce::Point<float> leftC (window.getX() + window.getHeight() * 0.5f, window.getCentreY());
    const juce::Point<float> rightC (window.getRight() - window.getHeight() * 0.5f, window.getCentreY());

    // Tape slowly moves from the left reel to the right one
    const float progress = 0.5f + 0.5f * (float) std::sin (clock * 0.02);
    paintReel (g, leftC, r, angle, 1.0f - progress);
    paintReel (g, rightC, r, angle * 1.1f, progress);

    // Dust specks on the shell
    const float dust = value (KngLoFiPlugin::dust);
    juce::Random rng ((juce::int64) (clock * 6.0));

    for (int k = 0; k < (int) (dust * 30.0f); ++k)
    {
        g.setColour (juce::Colours::white.withAlpha (0.15f + rng.nextFloat() * 0.3f));
        g.fillEllipse (shell.getX() + rng.nextFloat() * shell.getWidth(), shell.getY() + rng.nextFloat() * shell.getHeight(), 1.5f, 1.5f);
    }

    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText (juce::String (value (KngLoFiPlugin::bits), 1) + " BIT   " + juce::String (value (KngLoFiPlugin::rate) / 1000.0f, 1) + " kHz",
                readout, juce::Justification::centredLeft, false);
}
