#include "KngSamplerView.h"
#include "EditSession.h"
#include "TheoryHelper.h"
#include "KitBrowser.h"
#include "KngPopOut.h"

namespace te = tracktion;

static const juce::Colour violet (0xff9b7bff), lilac (0xffb9a1ff), deepViolet (0xff7a62d6), rootPink (0xffff9bd2),
                          paper (0xfff1ecff), muted (0xff6d6684), ink (0xff130c2b);

//==============================================================================
void KngSamplerView::resized()
{
    panel = getLocalBounds().reduced (8);
    auto inner = panel.reduced (20, 14);

    header = inner.removeFromTop (40);
    {
        auto h = header;
        popButton.setBounds (h.removeFromRight (32).withSizeKeepingCentre (32, 32));
        h.removeFromRight (10);
        loadButton.setBounds (h.removeFromRight (120).withSizeKeepingCentre (120, 32));
    }

    inner.removeFromTop (12);

    auto controls = inner.removeFromBottom (juce::jlimit (78, 100, inner.getHeight() / 3));
    inner.removeFromBottom (10);

    auto keys = inner.removeFromBottom (juce::jlimit (30, 56, inner.getHeight() / 4));
    keyboard.setBounds (keys);
    keyboard.setKeyWidth (juce::jmax (10.0f, (float) keys.getWidth() / 21.0f));
    inner.removeFromBottom (10);

    wave = inner;

    const int k = juce::jlimit (36, 54, controls.getHeight() - 34);

    for (auto& knob : knobs)
        knob->setBounds (controls.removeFromLeft (k + 44).withSizeKeepingCentre (k + 40, k + 32));

    auto pillRow = controls.removeFromRight (juce::jmin (controls.getWidth(), 3 * 108));

    for (auto& pill : pills)
        pill->setBounds (pillRow.removeFromLeft (108).withSizeKeepingCentre (100, 32));
}

int KngSamplerView::fracToX (float f) const
{
    const auto w = wave.reduced (16, 0);
    return w.getX() + juce::roundToInt (f * w.getWidth());
}

float KngSamplerView::xToFrac (int x) const
{
    const auto w = wave.reduced (16, 0);
    return juce::jlimit (0.0f, 1.0f, (x - w.getX()) / (float) juce::jmax (1, w.getWidth()));
}

//==============================================================================
void KngSamplerView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0b10));

    const auto p = panel.toFloat();
    juce::ColourGradient bg (juce::Colour (0xff1c1924), p.getX(), p.getY(), juce::Colour (0xff0f0d14), p.getRight(), p.getBottom(), false);
    bg.addColour (0.6, juce::Colour (0xff121018));
    g.setGradientFill (bg);
    g.fillRoundedRectangle (p, 18.0f);
    g.setColour (juce::Colour (0xff2a2635));
    g.drawRoundedRectangle (p.reduced (0.5f), 18.0f, 1.0f);

    paintHeader (g);
    paintWave (g);
}

void KngSamplerView::paintHeader (juce::Graphics& g)
{
    const auto h = header.toFloat();
    const float right = (float) loadButton.getX() - 12.0f;

    g.setFont (KngFonts::font (KngFonts::sora, 24.0f));
    g.setColour (paper);
    g.drawText ("KNG", juce::Rectangle<float> (h.getX(), h.getY(), 60.0f, h.getHeight()), juce::Justification::centredLeft, false);
    g.setColour (violet);
    g.drawText ("Sampler", juce::Rectangle<float> (h.getX() + 58.0f, h.getY(), 120.0f, h.getHeight()), juce::Justification::centredLeft, false);

    float x = h.getX() + 186.0f;

    if (x + 80.0f > right)
        return;

    // The sample's name in a pill
    auto name = plugin.getSampleFile().getFileName();

    if (name.isEmpty())
        name = "No sample yet";

    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    const float pillW = juce::jmin (300.0f, right - x, 40.0f + (float) name.length() * 7.5f);
    const auto pill = juce::Rectangle<float> (x, h.getCentreY() - 15.0f, pillW, 30.0f);

    g.setColour (juce::Colour (0xff0b0a0f));
    g.fillRoundedRectangle (pill, 8.0f);
    g.setColour (juce::Colour (0xff2a2635));
    g.drawRoundedRectangle (pill.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (violet);
    g.fillEllipse (pill.getX() + 12.0f, pill.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (juce::Colour (0xffd9ceff));
    g.drawText (name, pill.withTrimmedLeft (28.0f).withTrimmedRight (10.0f), juce::Justification::centredLeft, true);

    x = pill.getRight() + 12.0f;

    // Sample rate, length, channels
    if (auto buffer = plugin.getSampleForDisplay())
    {
        const double rate = juce::jmax (1.0, plugin.getFileSampleRate());
        const juce::String dot (juce::CharPointer_UTF8 (" \xc2\xb7 "));
        const auto info = juce::String (rate / 1000.0, 1) + " kHz" + dot
                        + juce::String (buffer->getNumSamples() / rate, 2) + " s" + dot
                        + (buffer->getNumChannels() > 1 ? "stereo" : "mono");

        if (right - x > 60.0f)
        {
            g.setFont (KngFonts::font (KngFonts::mono, 11.0f));
            g.setColour (muted);
            g.drawText (info, juce::Rectangle<float> (x, h.getY(), right - x, h.getHeight()), juce::Justification::centredLeft, true);
        }
    }
}

void KngSamplerView::paintWave (juce::Graphics& g)
{
    if (wave.getHeight() < 30)
        return;

    const auto w = wave.toFloat();
    g.setColour (hovering ? violet.withAlpha (0.12f) : juce::Colour (0xff0b0a0f));
    g.fillRoundedRectangle (w, 14.0f);
    g.setColour (hovering ? violet : juce::Colour (0xff241f30));
    g.drawRoundedRectangle (w.reduced (hovering ? 1.0f : 0.5f), 14.0f, hovering ? 2.0f : 1.0f);

    if (peaks.empty())
    {
        g.setFont (KngFonts::font (KngFonts::sora, 15.0f));
        g.setColour (muted);
        g.drawText ("Drop a sample here, or click Load sample", w, juce::Justification::centred, false);
        return;
    }

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (wave.reduced (1));

    const bool loopOn = (bool) plugin.state.getProperty (KngSamplerPlugin::loopProp, false);
    const float ls = propFloat (KngSamplerPlugin::loopStartProp, 0.4f);
    const float le = propFloat (KngSamplerPlugin::loopEndProp, 0.9f);

    // Loop region behind the bars
    if (loopOn)
    {
        const float x1 = (float) fracToX (ls), x2 = (float) fracToX (le);
        juce::ColourGradient region (violet.withAlpha (0.16f), 0.0f, w.getY(), violet.withAlpha (0.06f), 0.0f, w.getBottom(), false);
        g.setGradientFill (region);
        g.fillRect (x1, w.getY(), x2 - x1, w.getHeight());

        g.setColour (violet);
        g.fillRect (x1 - 1.0f, w.getY(), 2.0f, w.getHeight());
        g.fillRect (x2 - 1.0f, w.getY(), 2.0f, w.getHeight());

        g.setFont (KngFonts::font (KngFonts::mono, 10.0f, 0.1f));
        g.setColour (juce::Colour (0xffc4b2ff));
        g.drawText ("LOOP START", juce::Rectangle<float> (x1 + 8.0f, w.getY() + 8.0f, 90.0f, 14.0f), juce::Justification::centredLeft, false);
        g.drawText ("LOOP END", juce::Rectangle<float> (x2 + 8.0f, w.getY() + 8.0f, 80.0f, 14.0f), juce::Justification::centredLeft, false);
    }

    // The bars
    const auto area = w.reduced (16.0f, 28.0f);
    const int n = (int) peaks.size();
    const float step = area.getWidth() / (float) juce::jmax (1, n);

    for (int i = 0; i < n; ++i)
    {
        const float frac = (float) i / (float) n;
        const float hgt = juce::jmax (3.0f, peaks[(size_t) i] * area.getHeight());
        const bool inLoop = loopOn && frac >= ls && frac <= le;

        g.setColour (inLoop ? lilac : deepViolet);
        g.fillRoundedRectangle (area.getX() + i * step, area.getCentreY() - hgt * 0.5f, juce::jmax (1.5f, step - 1.0f), hgt, 1.5f);
    }

    // Playhead
    const float pos = plugin.displayPos.load();

    if (pos >= 0.0f && pos <= 1.0f)
    {
        const float px = (float) fracToX (pos);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.fillRect (px - 3.0f, w.getY(), 6.0f, w.getHeight());
        g.setColour (juce::Colours::white);
        g.fillRect (px - 1.0f, w.getY(), 2.0f, w.getHeight());
    }

    if (loopOn && w.getHeight() > 120.0f)
    {
        g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
        g.setColour (juce::Colour (0xff5d5672));
        g.drawText ("Drag the purple edges to set the loop", w.reduced (16.0f, 8.0f).removeFromBottom (14.0f),
                    juce::Justification::centredLeft, false);
    }
}
