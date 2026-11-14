#include "KngTransientPlugin.h"
#include "KngTransientView.h"

namespace te = tracktion;

const char* KngTransientPlugin::xmlTypeName = "kngtransient";

static std::vector<KngParamDef> transientParams()
{
    return {
        { "attack",  "Attack",  -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, "%",  nullptr },
        { "sustain", "Sustain", -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, "%",  nullptr },
        { "output",  "Output", -12.0f, 6.0f, 0.0f, 0.0f, 0.0f, "dB", nullptr },
        { "mix",     "Mix",     0.0f, 1.0f, 1.0f, 0.0f, 0.0f, "%",  nullptr }
    };
}

static std::vector<KngPresetDef> transientPresets()
{
    using P = KngTransientPlugin;

    return {
        { "Default", {} },
        { "Snappy Snare",  { { P::attack, 0.6f }, { P::sustain, -0.2f } } },
        { "Tight Kick",    { { P::attack, 0.4f }, { P::sustain, -0.6f } } },
        { "Room Bloom",    { { P::attack, -0.2f }, { P::sustain, 0.7f } } },
        { "Soft Hats",     { { P::attack, -0.5f } } },
        { "Punchy 808",    { { P::attack, 0.5f }, { P::sustain, 0.2f }, { P::output, -1.0f } } }
    };
}

KngTransientPlugin::KngTransientPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, transientParams(), transientPresets())
{
}

void KngTransientPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    reset();
}

void KngTransientPlugin::reset()
{
    fastEnv.reset();
    slowEnv.reset();
    holdEnv.reset();
    dropEnv.reset();
    gainDb = 0.0f;
}

float KngTransientPlugin::exampleGainDb (float t) const
{
    // A made-up hit: the first 10% is the attack, the rest is the ring
    const float attackPart = t < 0.1f ? 1.0f - t / 0.1f : 0.0f;
    const float sustainPart = t >= 0.1f ? juce::jlimit (0.0f, 1.0f, (t - 0.1f) / 0.3f) : 0.0f;
    return get (attack) * 12.0f * attackPart + get (sustain) * 12.0f * sustainPart;
}

void KngTransientPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    const double sr = sampleRate;
    const float fastA = KngDsp::Follower::coef (0.5, sr), fastR = KngDsp::Follower::coef (40.0, sr);
    const float slowA = KngDsp::Follower::coef (25.0, sr), slowR = KngDsp::Follower::coef (40.0, sr);
    const float holdA = KngDsp::Follower::coef (2.0, sr), holdR = KngDsp::Follower::coef (400.0, sr);
    const float dropA = KngDsp::Follower::coef (2.0, sr), dropR = KngDsp::Follower::coef (40.0, sr);
    const float smooth = KngDsp::Follower::coef (0.5, sr);

    const float attackAmount = get (attack), sustainAmount = get (sustain), mixAmount = get (mix);
    const float outGain = juce::Decibels::decibelsToGain (get (output));

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = chans > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;

    float inMax = 0.0f, outMax = 0.0f, moveMax = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float l = left[i];
        const float r = right != nullptr ? right[i] : l;
        const float level = juce::jmax (std::abs (l), std::abs (r));

        // Attack: the fast follower jumps ahead of the slow one at the start of each hit
        const float f = fastEnv.process (level, fastA, fastR);
        const float s = slowEnv.process (level, slowA, slowR);
        const float hit = juce::jlimit (0.0f, 1.0f, (f - s) / (f + 1.0e-6f));

        // Sustain: a slow-release follower stays up while the quick one falls away (the ring)
        const float h = holdEnv.process (level, holdA, holdR);
        const float d = dropEnv.process (level, dropA, dropR);
        const float ring = juce::jlimit (0.0f, 1.0f, (h - d) / (h + 1.0e-6f));

        const float target = juce::jlimit (-18.0f, 18.0f, attackAmount * 12.0f * hit + sustainAmount * 12.0f * ring);
        gainDb += (target - gainDb) * smooth;
        const float g = juce::Decibels::decibelsToGain (gainDb) * outGain;

        const float ol = l * (1.0f - mixAmount) + l * g * mixAmount;
        const float orr = r * (1.0f - mixAmount) + r * g * mixAmount;
        left[i] = ol;

        if (right != nullptr)
            right[i] = orr;

        inMax = juce::jmax (inMax, std::abs (l), std::abs (r));
        outMax = juce::jmax (outMax, std::abs (ol), std::abs (orr));

        if (std::abs (gainDb) > std::abs (moveMax))
            moveMax = gainDb;
    }

    // The third meter carries how far it moved the level (+ louder, - quieter), offset so it's never negative
    reportMeters (inMax, outMax, moveMax + 24.0f);
}

std::unique_ptr<te::Plugin::EditorComponent> KngTransientPlugin::createEditor()
{
    return std::make_unique<KngTransientView> (*this);
}
