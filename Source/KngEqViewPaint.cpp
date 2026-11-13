#include "KngEqView.h"
#include "KngEqPlugin.h"

namespace te = tracktion;

static juce::Colour bandColour (int band)
{
    static const juce::uint32 colours[KngEq::numBands] = {
        0xffff6b5a, 0xffffb44a, 0xffe8e04a, 0xff6ee87a, 0xff3ae6c8, 0xff4aa8ff, 0xffa48aff, 0xffff7ac8
    };

    return juce::Colour (colours[band]);
}

static juce::String freqText (double f)
{
    if (f >= 1000.0)
        return juce::String (f / 1000.0, f >= 10000.0 ? 1 : 2) + " kHz";

    return juce::String (juce::roundToInt (f)) + " Hz";
}

static bool isCut (int type)
{
    return type == KngEq::lowCut || type == KngEq::highCut;
}

//==============================================================================
void KngEqView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "EQ", KngFonts::spaceGrotesk, 28.0f);
}

void KngEqView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    const auto gr = graph();
    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (gr.toNearestInt());

    const double sr = eq.getSampleRate() > 0.0 ? eq.getSampleRate() : 48000.0;
    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));

    // Grid
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = freqToX (f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) x, gr.getY(), gr.getBottom());
        g.setColour (style.label.withAlpha (0.7f));
        g.drawText (freqText (f), juce::Rectangle<float> (x + 3.0f, gr.getBottom() - 15.0f, 60.0f, 12.0f), juce::Justification::left, false);
    }

    for (int db = -18; db <= 18; db += 6)
    {
        const float y = gainToY (db);
        g.setColour (juce::Colours::white.withAlpha (db == 0 ? 0.16f : 0.05f));
        g.drawHorizontalLine ((int) y, gr.getX(), gr.getRight());

        if (db != 0)
        {
            g.setColour (style.label.withAlpha (0.6f));
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (gr.getX() + 5.0f, y - 6.0f, 30.0f, 12.0f),
                        juce::Justification::left, false);
        }
    }

    // Live spectrum
    juce::Path spectrum;
    float lastX = gr.getX();

    for (float x = gr.getX(); x <= gr.getRight(); x += 2.0f)
    {
        const double bin = xToFreq (x) / sr * fftSize;
        const int i0 = (int) bin;

        if (i0 >= fftSize / 2 - 1)
            break;

        const float frac = (float) (bin - i0);
        const float db = spectrumDb[(size_t) i0] + frac * (spectrumDb[(size_t) i0 + 1] - spectrumDb[(size_t) i0]);
        const float y = gr.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f) * gr.getHeight();

        if (spectrum.isEmpty())
            spectrum.startNewSubPath (x, gr.getBottom());

        spectrum.lineTo (x, y);
        lastX = x;
    }

    if (! spectrum.isEmpty())
    {
        spectrum.lineTo (lastX, gr.getBottom());
        spectrum.closeSubPath();

        juce::ColourGradient fill (style.accent.withAlpha (0.22f), 0.0f, gr.getY(), style.accent.withAlpha (0.02f), 0.0f, gr.getBottom(), false);
        g.setGradientFill (fill);
        g.fillPath (spectrum);
        g.setColour (style.accent.withAlpha (0.35f));
        g.strokePath (spectrum, juce::PathStrokeType (1.0f));
    }

    // Each band's filter, then the total curve
    std::array<std::array<KngEq::Coefs, 4>, KngEq::numBands> bandCoefs {};
    std::array<int, KngEq::numBands> bandStages {};
    std::array<bool, KngEq::numBands> enabled {};

    for (int b = 0; b < KngEq::numBands; ++b)
    {
        enabled[(size_t) b] = value (b, KngEq::bandOn) > 0.5f;

        if (enabled[(size_t) b])
            bandStages[(size_t) b] = KngEq::makeBand (juce::roundToInt (value (b, KngEq::bandType)),
                                                      juce::roundToInt (value (b, KngEq::bandSlope)),
                                                      value (b, KngEq::bandFreq), value (b, KngEq::bandGain),
                                                      value (b, KngEq::bandQ), sr, bandCoefs[(size_t) b]);
    }

    auto bandDb = [&] (int b, double f)
    {
        double db = 0.0;

        for (int s = 0; s < bandStages[(size_t) b]; ++s)
            db += KngEq::magnitudeDb (bandCoefs[(size_t) b][(size_t) s], f, sr);

        return db;
    };

    for (int b = 0; b < KngEq::numBands; ++b)
    {
        if (! enabled[(size_t) b])
            continue;

        juce::Path curve;

        for (float x = gr.getX(); x <= gr.getRight(); x += 2.0f)
        {
            const float y = gainToY (juce::jlimit (-30.0, 30.0, bandDb (b, xToFreq (x))));

            if (curve.isEmpty())
                curve.startNewSubPath (x, y);
            else
                curve.lineTo (x, y);
        }

        g.setColour (bandColour (b).withAlpha (b == selected ? 0.85f : 0.4f));
        g.strokePath (curve, juce::PathStrokeType (1.2f));
    }

    juce::Path total;

    for (float x = gr.getX(); x <= gr.getRight(); x += 2.0f)
    {
        double db = 0.0;
        const double f = xToFreq (x);

        for (int b = 0; b < KngEq::numBands; ++b)
            if (enabled[(size_t) b])
                db += bandDb (b, f);

        const float y = gainToY (juce::jlimit (-30.0, 30.0, db));

        if (total.isEmpty())
            total.startNewSubPath (x, y);
        else
            total.lineTo (x, y);
    }

    g.setColour (style.accent.withAlpha (0.3f));
    g.strokePath (total, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.strokePath (total, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Band dots
    bool anyEnabled = false;
    g.setFont (KngFonts::font (KngFonts::mono, 11.0f));

    for (int b = 0; b < KngEq::numBands; ++b)
    {
        if (! enabled[(size_t) b])
            continue;

        anyEnabled = true;
        const auto dot = juce::Rectangle<float> (20.0f, 20.0f).withCentre (handlePos (b));

        g.setColour (bandColour (b).withAlpha (0.3f));
        g.fillEllipse (dot.expanded (4.0f));
        g.setColour (bandColour (b));
        g.fillEllipse (dot);

        if (b == selected)
        {
            g.setColour (juce::Colours::white);
            g.drawEllipse (dot.expanded (3.0f), 2.0f);
        }

        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawText (juce::String (b + 1), dot, juce::Justification::centred, false);
    }

    // What's selected, top left
    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    const auto info = gr.reduced (12.0f, 10.0f).removeFromTop (16.0f);

    if (selected >= 0)
    {
        const int type = juce::roundToInt (value (selected, KngEq::bandType));
        auto text = "BAND " + juce::String (selected + 1) + "  " + KngEq::typeNames()[type].toUpperCase();

        if (isCut (type))
            text << "  " << KngEq::slopeNames()[juce::roundToInt (value (selected, KngEq::bandSlope))];

        g.setColour (bandColour (selected));
        g.drawText (text, info, juce::Justification::centredLeft, false);
    }
    else if (anyEnabled)
    {
        g.setColour (style.label);
        g.drawText ("Click a dot to edit it. Double-click empty space to add a band.", info, juce::Justification::centredLeft, false);
    }

    if (! anyEnabled)
    {
        g.setColour (style.label);
        g.setFont (KngFonts::font (KngFonts::spaceGrotesk, 15.0f));
        g.drawText ("Double-click to add a band. Drag to move it, scroll to change its width, right-click for options.",
                    gr.reduced (20.0f), juce::Justification::centred, true);
    }
}
