#include "KngRadioPlugin.h"
#include "KngRadioView.h"

namespace te = tracktion;

const char* KngRadioPlugin::xmlTypeName = "kngradio";

static std::vector<KngParamDef> radioParams()
{
    return {
        { "band",  "Sound", 0.0f,  3.0f, 1.0f, 0.0f, 1.0f, "",   "Phone|Radio|Megaphone|Walkie" },
        { "tone",  "Tone", -1.0f,  1.0f, 0.0f, 0.0f, 0.0f, "",   nullptr },
        { "drive", "Drive", 0.0f, 24.0f, 8.0f, 0.0f, 0.0f, "dB", nullptr },
        { "crush", "Crush", 0.0f,  1.0f, 0.2f, 0.0f, 0.0f, "%",  nullptr },
        { "noise", "Noise", 0.0f,  1.0f, 0.1f, 0.0f, 0.0f, "%",  nullptr },
        { "mix",   "Mix",   0.0f,  1.0f, 1.0f, 0.0f, 0.0f, "%",  nullptr }
    };
}

static std::vector<KngPresetDef> radioPresets()
{
    using P = KngRadioPlugin;

    return {
        { "Default", {} },
        { "Phone Call",    { { P::band, 0.0f }, { P::drive, 10.0f }, { P::crush, 0.25f }, { P::noise, 0.05f } } },
        { "Old Radio",     { { P::band, 1.0f }, { P::crush, 0.3f }, { P::noise, 0.25f } } },
        { "Megaphone",     { { P::band, 2.0f }, { P::drive, 16.0f }, { P::crush, 0.1f }, { P::noise, 0.0f } } },
        { "Walkie Talkie", { { P::band, 3.0f }, { P::drive, 12.0f }, { P::crush, 0.5f }, { P::noise, 0.3f } } },
        { "Intro Filter",  { { P::band, 1.0f }, { P::drive, 4.0f }, { P::crush, 0.0f }, { P::noise, 0.0f }, { P::mix, 0.6f } } }
    };
}

KngRadioPlugin::Shape KngRadioPlugin::shapeFor (int b)
{
    switch (b)
    {
        case 0:  return { 400.0f, 3400.0f, 1800.0f, 3.0f };   // Phone
        case 2:  return { 700.0f, 2800.0f, 1500.0f, 8.0f };   // Megaphone
        case 3:  return { 500.0f, 2500.0f, 1200.0f, 6.0f };   // Walkie
        default: return { 250.0f, 5000.0f, 1800.0f, 4.0f };   // Radio
    }
}

KngRadioPlugin::KngRadioPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, radioParams(), radioPresets())
{
}

void KngRadioPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    lastRate = 0.0;
    reset();
}

void KngRadioPlugin::reset()
{
    for (auto& chain : filters)
        for (auto& f : chain)
            f.reset();

    held = {};
    holdCount = {};
}

void KngRadioPlugin::getBandEdges (float& lowHz, float& highHz) const
{
    const auto s = shapeFor (juce::roundToInt (get (band)));
    const float shift = std::pow (2.0f, get (tone));
    lowHz = s.low * shift;
    highHz = s.high * shift;
}

void KngRadioPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    if (get (band) != lastBand || get (tone) != lastTone || sampleRate != lastRate)
    {
        const auto s = shapeFor (juce::roundToInt (get (band)));
        const double shift = std::pow (2.0, (double) get (tone));

        // Two high passes and two low passes for steep edges, then the band's honk
        for (auto& chain : filters)
        {
            chain[0].highPass (sampleRate, s.low * shift, 0.9);
            chain[1].highPass (sampleRate, s.low * shift, 0.6);
            chain[2].lowPass (sampleRate, s.high * shift, 0.9);
            chain[3].lowPass (sampleRate, s.high * shift, 0.6);
            chain[4].peak (sampleRate, s.bellHz * shift, s.bellDb, 1.0);
        }

        lastBand = get (band);
        lastTone = get (tone);
        lastRate = sampleRate;
    }

    const float driveGain = juce::Decibels::decibelsToGain (get (drive));
    const float driveNorm = 1.0f / std::sqrt (driveGain);
    const float crushAmount = get (crush), noiseAmount = get (noise), mixAmount = get (mix);
    const float holdLength = 1.0f + crushAmount * 7.0f;
    const float steps = std::pow (2.0f, 16.0f - 12.0f * crushAmount);

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    float inMax = 0.0f, outMax = 0.0f;

    for (int c = 0; c < chans; ++c)
    {
        auto* d = buffer.getWritePointer (c, fc.bufferStartSample);
        auto& chain = filters[(size_t) c];
        auto& h = held[(size_t) c];
        auto& count = holdCount[(size_t) c];

        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float dry = d[i];
            float x = dry;

            // Hiss and the odd crackle go in first, so the band shapes them too
            if (noiseAmount > 0.0f)
            {
                x += (random.nextFloat() * 2.0f - 1.0f) * noiseAmount * 0.04f;

                if (random.nextFloat() < noiseAmount * 0.0005f)
                    x += (random.nextFloat() * 2.0f - 1.0f) * noiseAmount * 0.5f;
            }

            for (auto& f : chain)
                x = f.process (x);

            x = std::tanh (x * driveGain) * driveNorm;

            // Crush: fewer samples and fewer steps
            count += 1.0f;

            if (count >= holdLength)
            {
                count -= holdLength;
                h = std::round (x * steps) / steps;
            }

            const float y = dry * (1.0f - mixAmount) + h * mixAmount;
            d[i] = y;

            inMax = juce::jmax (inMax, std::abs (dry));
            outMax = juce::jmax (outMax, std::abs (y));
        }
    }

    reportMeters (inMax, outMax, 0.0f);
}

std::unique_ptr<te::Plugin::EditorComponent> KngRadioPlugin::createEditor()
{
    return std::make_unique<KngRadioView> (*this);
}
