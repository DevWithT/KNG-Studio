#include "KngAirPlugin.h"
#include "KngAirView.h"

namespace te = tracktion;

const char* KngAirPlugin::xmlTypeName = "kngair";

static std::vector<KngParamDef> airParams()
{
    return {
        { "air",      "Air",          0.0f,    12.0f,     4.0f,     0.0f, 0.0f, "dB", nullptr },
        { "freq",     "Frequency", 6000.0f, 18000.0f, 12000.0f, 11000.0f, 0.0f, "Hz", nullptr },
        { "presence", "Presence",    -6.0f,     6.0f,     0.0f,     0.0f, 0.0f, "dB", nullptr },
        { "shine",    "Shine",        0.0f,     1.0f,     0.2f,     0.0f, 0.0f, "%",  nullptr },
        { "output",   "Output",     -12.0f,     6.0f,     0.0f,     0.0f, 0.0f, "dB", nullptr }
    };
}

static std::vector<KngPresetDef> airPresets()
{
    using P = KngAirPlugin;

    return {
        { "Default", {} },
        { "Silky Top",   { { P::air, 5.0f }, { P::freq, 14000.0f }, { P::shine, 0.1f } } },
        { "Radio Ready", { { P::air, 6.0f }, { P::presence, 2.5f }, { P::shine, 0.35f } } },
        { "Breathy R&B", { { P::air, 8.0f }, { P::freq, 12000.0f }, { P::presence, -1.5f }, { P::shine, 0.15f } } },
        { "Cut Through", { { P::air, 3.0f }, { P::presence, 4.0f }, { P::shine, 0.4f } } },
        { "Just Shine",  { { P::air, 0.0f }, { P::shine, 0.6f } } }
    };
}

KngAirPlugin::KngAirPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, airParams(), airPresets())
{
}

void KngAirPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    lastRate = 0.0;
    reset();
}

void KngAirPlugin::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        shelf[(size_t) c].reset();
        bell[(size_t) c].reset();
        shineHp[(size_t) c].reset();
    }
}

void KngAirPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    const float a = get (air), f = get (freq), p = get (presence);

    if (a != lastAir || f != lastFreq || p != lastPresence || sampleRate != lastRate)
    {
        for (int c = 0; c < 2; ++c)
        {
            shelf[(size_t) c].highShelf (sampleRate, f, a, 0.6);
            bell[(size_t) c].peak (sampleRate, 4500.0, p, 0.8);
            shineHp[(size_t) c].highPass (sampleRate, 6000.0);
        }

        lastAir = a;
        lastFreq = f;
        lastPresence = p;
        lastRate = sampleRate;
    }

    const float shineAmount = get (shine);
    const float outGain = juce::Decibels::decibelsToGain (get (output));

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    float inMax = 0.0f, outMax = 0.0f;

    for (int c = 0; c < chans; ++c)
    {
        auto* d = buffer.getWritePointer (c, fc.bufferStartSample);
        auto& sh = shelf[(size_t) c];
        auto& bl = bell[(size_t) c];
        auto& hp = shineHp[(size_t) c];

        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float x = d[i];
            float y = bl.process (sh.process (x));

            // Shine: the top end, lightly saturated and added back for a little sparkle
            const float h = hp.process (x);
            y += shineAmount * std::tanh (h * 5.0f) * 0.2f;

            y *= outGain;
            d[i] = y;

            inMax = juce::jmax (inMax, std::abs (x));
            outMax = juce::jmax (outMax, std::abs (y));
        }
    }

    reportMeters (inMax, outMax, 0.0f);
}

float KngAirPlugin::curveAt (float hz) const
{
    const float shelfPart = get (air) / (1.0f + std::pow (get (freq) / hz, 2.0f));
    const float oct = std::log2 (hz / 4500.0f);
    const float bellPart = get (presence) * std::exp (-oct * oct * 2.0f);
    const float shinePart = get (shine) * 3.0f / (1.0f + std::pow (6000.0f / hz, 4.0f));
    return shelfPart + bellPart + shinePart + get (output);
}

std::unique_ptr<te::Plugin::EditorComponent> KngAirPlugin::createEditor()
{
    return std::make_unique<KngAirView> (*this);
}
