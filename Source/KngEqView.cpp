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

static bool hasGain (int type)
{
    return type == KngEq::lowShelf || type == KngEq::bell || type == KngEq::highShelf;
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
