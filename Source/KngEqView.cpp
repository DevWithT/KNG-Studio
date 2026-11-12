#include "KngEqView.h"
#include "KngEqPlugin.h"

namespace te = tracktion;

static constexpr double gainRange = 24.0;

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

static bool hasGain (int type)
{
    return type == KngEq::lowShelf || type == KngEq::bell || type == KngEq::highShelf;
}

static bool isCut (int type)
{
    return type == KngEq::lowCut || type == KngEq::highCut;
}

static KngParamDef eqDef (int paramIndex, const char* unit)
{
    const auto& p = KngEq::params()[(size_t) paramIndex];
    return { "", "", (float) p.min, (float) p.max, (float) p.def, (float) p.mid, (float) p.step, unit, nullptr };
}

//==============================================================================
KngEqView::KngEqView (KngEqPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xff2fbf71, 0xff2a3a33, 0xff0e1512, 0xffe8fff3, 0xff8fb5a3, 0xff7ff0b5, 0xff0f1a16, 0xff23382f,
                                 KngFonts::spaceGrotesk, false),
                   { juce::Colour (0xff070b0a), juce::Colour (0xff14201b), juce::Colour (0xff0b1310), juce::Colour (0xff1f332b) }),
      eq (owner)
{
    titleWidth = 150;
    attachPresets ({}, [] (int) {});
    rebuildKnobs();
}

void KngEqView::beginEdit()
{
    if (auto* um = eq.getUndoManager())
        um->beginNewTransaction();
}

float KngEqView::value (int band, int p) const
{
    return eq.getParam (KngEq::index (band, p))->getCurrentValue();
}

void KngEqView::setValue (int band, int p, float v)
{
    eq.getParam (KngEq::index (band, p))->setParameter (v, juce::sendNotification);
}

//==============================================================================
void KngEqView::rebuildKnobs()
{
    clearKnobs();
    gainKnob = nullptr;
    builtFor = selected;

    // The selected band's knobs, in its own color
    if (selected >= 0)
    {
        const int fi = KngEq::index (selected, KngEq::bandFreq);
        const int gi = KngEq::index (selected, KngEq::bandGain);
        const int qi = KngEq::index (selected, KngEq::bandQ);

        auto& f = addKnob (*eq.getParam (fi), eqDef (fi, "Hz"), "Freq");
        auto& gk = addKnob (*eq.getParam (gi), eqDef (gi, "dB"), "Gain");
        auto& q = addKnob (*eq.getParam (qi), eqDef (qi, ""), "Width (Q)");
        q.formatter = [] (float v) { return juce::String (v, 2); };

        for (auto* k : { &f, &gk, &q })
            k->setArcColour (bandColour (selected));

        gainKnob = &gk;
    }

    addKnob (*eq.getParam (KngEq::outputParam), eqDef (KngEq::outputParam, "dB"), "Output");
    resized();
}

void KngEqView::selectBand (int band)
{
    selected = band;

    if (builtFor != band)
        rebuildKnobs();

    repaint();
}

void KngEqView::tick()
{
    updateSpectrum();

    if (selected >= 0 && value (selected, KngEq::bandOn) < 0.5f)
        selected = -1;

    if (selected != builtFor)
        rebuildKnobs();

    // Cuts have no gain
    if (gainKnob != nullptr && selected >= 0)
    {
        const bool g = hasGain (juce::roundToInt (value (selected, KngEq::bandType)));
        gainKnob->setEnabled (g);
        gainKnob->setAlpha (g ? 1.0f : 0.35f);
    }
}

void KngEqView::updateSpectrum()
{
    const int n = eq.pullAnalyserSamples (pullBuffer.data(), (int) pullBuffer.size());

    for (int i = 0; i < n; ++i)
    {
        ring[(size_t) ringPos] = pullBuffer[(size_t) i];
        ringPos = (ringPos + 1) % fftSize;
    }

    if (n > 0)
    {
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)];

        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());
    }

    // Rise instantly, fall slowly, so it reads like a meter
    for (int i = 0; i < fftSize / 2; ++i)
    {
        const float db = n > 0 ? juce::Decibels::gainToDecibels (fftData[(size_t) i] / (fftSize * 0.25f), -100.0f) : -100.0f;
        auto& s = spectrumDb[(size_t) i];
        s = db > s ? db : s - 1.2f;
    }
}

//==============================================================================
float KngEqView::freqToX (double f) const
{
    const auto gr = graph();
    return gr.getX() + gr.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0));
}

double KngEqView::xToFreq (float x) const
{
    const auto gr = graph();
    const double t = (x - gr.getX()) / juce::jmax (1.0f, gr.getWidth());
    return juce::jlimit (20.0, 20000.0, 20.0 * std::pow (1000.0, t));
}

float KngEqView::gainToY (double db) const
{
    const auto gr = graph();
    const float half = gr.getHeight() * 0.5f - 12.0f;
    return gr.getCentreY() - (float) (db / gainRange) * half;
}

double KngEqView::yToGain (float y) const
{
    const auto gr = graph();
    const float half = gr.getHeight() * 0.5f - 12.0f;
    return juce::jlimit (-gainRange, gainRange, (gr.getCentreY() - y) / juce::jmax (1.0f, half) * gainRange);
}

juce::Point<float> KngEqView::handlePos (int band) const
{
    const int type = juce::roundToInt (value (band, KngEq::bandType));
    return { freqToX (value (band, KngEq::bandFreq)), gainToY (hasGain (type) ? value (band, KngEq::bandGain) : 0.0) };
}

int KngEqView::bandAt (juce::Point<float> p) const
{
    for (int b = KngEq::numBands; --b >= 0;)
        if (value (b, KngEq::bandOn) > 0.5f && handlePos (b).getDistanceFrom (p) <= 11.0f)
            return b;

    return -1;
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

//==============================================================================
void KngEqView::mouseDown (const juce::MouseEvent& e)
{
    dragging = -1;

    if (! graph().contains (e.position))
        return;

    const int b = bandAt (e.position);

    if (e.mods.isPopupMenu())
    {
        if (b >= 0)
            showBandMenu (b);

        return;
    }

    if (b >= 0)
    {
        selectBand (b);
        dragging = b;
        beginEdit();
    }
}

void KngEqView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    const int type = juce::roundToInt (value (dragging, KngEq::bandType));
    setValue (dragging, KngEq::bandFreq, (float) xToFreq (e.position.x));

    if (hasGain (type))
        setValue (dragging, KngEq::bandGain, (float) yToGain (e.position.y));

    repaint();
}

void KngEqView::mouseUp (const juce::MouseEvent&)
{
    dragging = -1;
}

void KngEqView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! graph().contains (e.position))
        return;

    // Double-click a dot to remove that band
    const int hit = bandAt (e.position);

    if (hit >= 0)
    {
        beginEdit();
        setValue (hit, KngEq::bandOn, 0.0f);
        selectBand (-1);
        return;
    }

    // Double-click empty space to add a band there
    const double f = xToFreq (e.position.x);
    const int type = f < 40.0 ? KngEq::lowCut : f > 15000.0 ? KngEq::highCut : KngEq::bell;

    std::vector<int> order;

    if (type == KngEq::lowCut)       order = { 0, 1, 2, 3, 4, 5, 6, 7 };
    else if (type == KngEq::highCut) order = { 7, 6, 5, 4, 3, 2, 1, 0 };
    else                             order = { 2, 3, 4, 5, 1, 6, 0, 7 };

    int band = -1;

    for (int b : order)
    {
        if (value (b, KngEq::bandOn) < 0.5f)
        {
            band = b;
            break;
        }
    }

    if (band < 0)
        return;   // all 8 bands in use

    beginEdit();
    setValue (band, KngEq::bandType, (float) type);
    setValue (band, KngEq::bandFreq, (float) f);
    setValue (band, KngEq::bandGain, type == KngEq::bell ? (float) yToGain (e.position.y) : 0.0f);
    setValue (band, KngEq::bandQ, type == KngEq::bell ? 1.0f : 0.707f);
    setValue (band, KngEq::bandSlope, 0.0f);
    setValue (band, KngEq::bandOn, 1.0f);
    selectBand (band);
}

void KngEqView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int b = bandAt (e.position);

    if (b < 0)
        b = selected;

    if (b < 0 || wheel.deltaY == 0.0f)
        return;

    // Scroll up for narrower, down for wider
    const float q = value (b, KngEq::bandQ) * (wheel.deltaY > 0.0f ? 1.15f : 1.0f / 1.15f);
    setValue (b, KngEq::bandQ, juce::jlimit (0.1f, 18.0f, q));
    repaint();
}

void KngEqView::showBandMenu (int band)
{
    juce::Component::SafePointer<KngEqView> safe (this);
    const int type = juce::roundToInt (value (band, KngEq::bandType));
    juce::PopupMenu menu;

    for (int t = 0; t < KngEq::typeNames().size(); ++t)
    {
        menu.addItem (KngEq::typeNames()[t], true, t == type, [safe, band, t]
        {
            if (safe == nullptr) return;
            safe->beginEdit();
            safe->setValue (band, KngEq::bandType, (float) t);
        });
    }

    if (isCut (type))
    {
        menu.addSeparator();
        const int slope = juce::roundToInt (value (band, KngEq::bandSlope));

        for (int s = 0; s < KngEq::slopeNames().size(); ++s)
        {
            menu.addItem (KngEq::slopeNames()[s], true, s == slope, [safe, band, s]
            {
                if (safe == nullptr) return;
                safe->beginEdit();
                safe->setValue (band, KngEq::bandSlope, (float) s);
            });
        }
    }

    menu.addSeparator();
    menu.addItem ("Remove Band", [safe, band]
    {
        if (safe == nullptr) return;
        safe->beginEdit();
        safe->setValue (band, KngEq::bandOn, 0.0f);
        safe->selectBand (-1);
    });

    menu.showMenuAsync ({});
}
