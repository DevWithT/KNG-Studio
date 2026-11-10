#include "KngDeEssPlugin.h"

namespace te = tracktion;

const char* KngDeEssPlugin::xmlTypeName = "kngdeess";

static std::vector<KngParamDef> deEssParams()
{
    return {
        { "freq",      "Frequency", 2000.0f, 12000.0f, 6500.0f, 6000.0f, 0.0f, "Hz", nullptr },
        { "threshold", "Threshold",  -60.0f,     0.0f,  -24.0f,    0.0f, 0.0f, "dB", nullptr },
        { "range",     "Range",        0.0f,    24.0f,    8.0f,    0.0f, 0.0f, "dB", nullptr },
        { "mode",      "Mode",         0.0f,     1.0f,    0.0f,    0.0f, 1.0f, "",   "Split|Wide" },
        { "listen",    "Listen",       0.0f,     1.0f,    0.0f,    0.0f, 1.0f, "",   "toggle" }
    };
}

static std::vector<KngPresetDef> deEssPresets()
{
    using P = KngDeEssPlugin;

    return {
        { "Default", {} },
        { "Male Vocal",   { { P::freq, 5500.0f }, { P::threshold, -26.0f }, { P::range, 8.0f } } },
        { "Female Vocal", { { P::freq, 7500.0f }, { P::threshold, -26.0f }, { P::range, 8.0f } } },
        { "Gentle",       { { P::threshold, -20.0f }, { P::range, 4.0f } } },
        { "Heavy",        { { P::threshold, -32.0f }, { P::range, 12.0f } } },
        { "Hi-Hat Tamer", { { P::freq, 9000.0f }, { P::threshold, -24.0f }, { P::range, 6.0f } } }
    };
}

KngDeEssPlugin::KngDeEssPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, deEssParams(), deEssPresets())
{
}

void KngDeEssPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    lastRate = 0.0;
    reset();
}

void KngDeEssPlugin::reset()
{
    for (auto& ch : detectState)
        for (auto& s : ch)
            s.reset();

    for (auto& s : splitState)
        s.reset();

    env = 0.0f;
}

void KngDeEssPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    const float f = get (freq);

    if (f != lastFreq || sampleRate != lastRate)
    {
        // Detector: a steep high pass just under the chosen frequency
        KngEq::makeBand (KngEq::lowCut, 1, f * 0.8, 0.0, 0.707, sampleRate, detectCoefs);

        // Split: the highs are whatever a low pass at that frequency leaves out
        splitCoefs = KngEq::makeBiquad (KngEq::highCut, f, 0.0, 0.707, sampleRate);
        lastFreq = f;
        lastRate = sampleRate;
    }

    const float thr = get (threshold), rng = get (range);
    const bool wide = get (mode) > 0.5f;
    const bool listening = get (listen) > 0.5f;
    const float attack  = 1.0f - std::exp (-1.0f / (0.0005f * (float) sampleRate));
    const float release = 1.0f - std::exp (-1.0f / (0.06f * (float) sampleRate));

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = chans > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;

    float inMax = 0.0f, outMax = 0.0f, grMax = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float l = left[i];
        const float r = right != nullptr ? right[i] : l;

        const float dl = detectState[0][1].process (detectState[0][0].process (l, detectCoefs[0]), detectCoefs[1]);
        const float dr = right != nullptr ? detectState[1][1].process (detectState[1][0].process (r, detectCoefs[0]), detectCoefs[1]) : dl;

        const float level = juce::jmax (std::abs (dl), std::abs (dr));
        env += (level - env) * (level > env ? attack : release);

        const float over = juce::Decibels::gainToDecibels (env, -120.0f) - thr;
        const float grDb = juce::jmin (rng, juce::jmax (0.0f, over * 0.8f));
        const float g = juce::Decibels::decibelsToGain (-grDb);

        float ol, orr;

        if (listening)
        {
            ol = dl;
            orr = dr;
        }
        else if (wide)
        {
            ol = l * g;
            orr = r * g;
        }
        else
        {
            const float lowL = splitState[0].process (l, splitCoefs);
            const float lowR = right != nullptr ? splitState[1].process (r, splitCoefs) : lowL;
            ol = lowL + (l - lowL) * g;
            orr = lowR + (r - lowR) * g;
        }

        left[i] = ol;

        if (right != nullptr)
            right[i] = orr;

        inMax = juce::jmax (inMax, std::abs (l), std::abs (r));
        outMax = juce::jmax (outMax, std::abs (ol), std::abs (orr));
        grMax = juce::jmax (grMax, grDb);
    }

    reportMeters (inMax, outMax, grMax);
}

void KngDeEssPlugin::drawDisplay (juce::Graphics& g, juce::Rectangle<float> area, const KngHistory& h)
{
    kngDrawHistory (g, area, h, 24.0f);

    const float f = get (freq);
    const auto text = f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("Watching above " + text + (get (mode) > 0.5f ? "  (Wide)" : "  (Split)"),
                area.reduced (12.0f).toNearestInt(), juce::Justification::topLeft);
}
