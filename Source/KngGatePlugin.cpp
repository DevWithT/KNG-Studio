#include "KngGatePlugin.h"
#include "KngGateView.h"

namespace te = tracktion;

const char* KngGatePlugin::xmlTypeName = "knggate";

static std::vector<KngParamDef> gateParams()
{
    return {
        { "threshold", "Threshold", -80.0f,    0.0f, -45.0f,   0.0f, 0.0f, "dB", nullptr },
        { "range",     "Range",       0.0f,   80.0f,  40.0f,   0.0f, 0.0f, "dB", nullptr },
        { "attack",    "Attack",      0.1f,   50.0f,   1.0f,   5.0f, 0.0f, "ms", nullptr },
        { "hold",      "Hold",        0.0f,  500.0f,  40.0f, 100.0f, 0.0f, "ms", nullptr },
        { "release",   "Release",     5.0f, 1000.0f, 150.0f, 200.0f, 0.0f, "ms", nullptr },
        { "lowCut",    "Low Cut",    20.0f, 1000.0f,  80.0f, 200.0f, 0.0f, "Hz", nullptr },
        { "mode",      "Mode",        0.0f,    1.0f,   0.0f,   0.0f, 1.0f, "",   "Gate|Soft" }
    };
}

static std::vector<KngPresetDef> gatePresets()
{
    using P = KngGatePlugin;

    return {
        { "Default", {} },
        { "Vocal Clean-Up", { { P::threshold, -50.0f }, { P::range, 18.0f }, { P::mode, 1.0f } } },
        { "Tight Vocal",    { { P::threshold, -40.0f }, { P::range, 40.0f }, { P::hold, 30.0f }, { P::release, 80.0f } } },
        { "Kill the Bleed", { { P::threshold, -35.0f }, { P::range, 60.0f }, { P::lowCut, 150.0f } } },
        { "Drum Gate",      { { P::threshold, -30.0f }, { P::range, 80.0f }, { P::attack, 0.2f }, { P::hold, 20.0f }, { P::release, 60.0f } } },
        { "Breath Tamer",   { { P::threshold, -45.0f }, { P::range, 10.0f }, { P::mode, 1.0f }, { P::release, 250.0f } } }
    };
}

KngGatePlugin::KngGatePlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, gateParams(), gatePresets())
{
}

void KngGatePlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    lastRate = 0.0;
    reset();
}

void KngGatePlugin::reset()
{
    detectHp.reset();
    detectEnv = 0.0f;
    gainDb = 0.0f;
    open = false;
    holdLeft = 0;
    openNow = false;
}

void KngGatePlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    if (get (lowCut) != lastLowCut || sampleRate != lastRate)
    {
        detectHp.highPass (sampleRate, get (lowCut));
        lastLowCut = get (lowCut);
        lastRate = sampleRate;
    }

    const float thr = get (threshold), rangeDb = get (range);
    const bool soft = get (mode) > 0.5f;
    const int holdSamples = (int) (get (hold) * 0.001 * sampleRate);

    // The detector follows peaks quickly; the gain moves at the Attack and Release you set
    const float detAttack  = KngDsp::Follower::coef (0.1, sampleRate);
    const float detRelease = KngDsp::Follower::coef (30.0, sampleRate);
    const float openSpeed  = KngDsp::Follower::coef (get (attack), sampleRate);
    const float closeSpeed = KngDsp::Follower::coef (get (release), sampleRate);
    constexpr float hysteresis = 4.0f;

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = chans > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;

    float inMax = 0.0f, outMax = 0.0f, grMax = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float l = left[i];
        const float r = right != nullptr ? right[i] : l;

        const float level = std::abs (detectHp.process (0.5f * (l + r)));
        detectEnv += (level - detectEnv) * (level > detectEnv ? detAttack : detRelease);
        const float envDb = juce::Decibels::gainToDecibels (detectEnv, -120.0f);

        float targetDb = 0.0f;

        if (soft)
        {
            // Expander: the further below Threshold, the quieter, up to Range
            targetDb = -juce::jmin (rangeDb, juce::jmax (0.0f, thr - envDb));
        }
        else
        {
            if (envDb > thr)
            {
                open = true;
                holdLeft = holdSamples;
            }
            else if (holdLeft > 0)
            {
                --holdLeft;
            }
            else if (envDb < thr - hysteresis)
            {
                open = false;
            }

            targetDb = open ? 0.0f : -rangeDb;
        }

        gainDb += (targetDb - gainDb) * (targetDb > gainDb ? openSpeed : closeSpeed);
        const float g = juce::Decibels::decibelsToGain (gainDb, -120.0f);

        const float ol = l * g, orr = r * g;
        left[i] = ol;

        if (right != nullptr)
            right[i] = orr;

        inMax = juce::jmax (inMax, std::abs (l), std::abs (r));
        outMax = juce::jmax (outMax, std::abs (ol), std::abs (orr));
        grMax = juce::jmax (grMax, -gainDb);
    }

    openNow = gainDb > -3.0f;
    reportMeters (inMax, outMax, grMax);
}

std::unique_ptr<te::Plugin::EditorComponent> KngGatePlugin::createEditor()
{
    return std::make_unique<KngGateView> (*this);
}
