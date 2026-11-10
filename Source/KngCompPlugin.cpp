#include "KngCompPlugin.h"
#include "KngCompView.h"

namespace te = tracktion;

const char* KngCompPlugin::xmlTypeName = "kngcomp";

static void raiseTo (std::atomic<float>& a, float v)
{
    float current = a.load();

    while (v > current && ! a.compare_exchange_weak (current, v)) {}
}

KngCompPlugin::KngCompPlugin (te::PluginCreationInfo creationInfo)
    : te::Plugin (creationInfo)
{
    auto* um = getUndoManager();

    for (int i = 0; i < KngComp::numParams; ++i)
    {
        const auto& p = KngComp::info (i);
        juce::NormalisableRange<float> range (p.min, p.max, p.step);

        if (p.mid > p.min && p.mid < p.max)
            range.setSkewForCentre (p.mid);

        auto* value = values.add (new juce::CachedValue<float>());
        value->referTo (state, p.id, um, p.def);

        auto* param = addParam (p.id, p.name, range);
        param->attachToCurrentValue (*value);
        params[(size_t) i] = param;
    }
}

KngCompPlugin::~KngCompPlugin()
{
    notifyListenersOfDeletion();

    for (auto& p : params)
        if (p != nullptr)
            p->detachFromCurrentValue();
}

std::unique_ptr<te::Plugin::EditorComponent> KngCompPlugin::createEditor()
{
    return std::make_unique<KngCompView> (*this);
}

void KngCompPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    lastRate = 0.0;
    reset();
}

void KngCompPlugin::reset()
{
    grDb = 0.0f;

    for (auto& s : scState)
        s.reset();
}

void KngCompPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    for (int i = 0; i < KngComp::numParams; ++i)
    {
        const juce::Identifier id (KngComp::info (i).id);

        if (v.hasProperty (id))
            values[i]->setValue ((float) v.getProperty (id), nullptr);
    }

    for (auto* p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

void KngCompPlugin::applyPreset (int presetIndex)
{
    const auto& list = KngComp::presets();

    if (! juce::isPositiveAndBelow (presetIndex, (int) list.size()))
        return;

    for (int i = 0; i < KngComp::numParams; ++i)
        params[(size_t) i]->setParameter (KngComp::info (i).def, juce::sendNotification);

    for (auto& [p, value] : list[(size_t) presetIndex].values)
        params[(size_t) p]->setParameter (value, juce::sendNotification);
}

void KngCompPlugin::takeMeters (float& inputPeak, float& outputPeak, float& reductionDb)
{
    inputPeak   = inPeak.exchange (0.0f);
    outputPeak  = outPeak.exchange (0.0f);
    reductionDb = grPeak.exchange (0.0f);
}

void KngCompPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    using namespace KngComp;

    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    float v[numParams];

    for (int i = 0; i < numParams; ++i)
        v[i] = params[(size_t) i]->getCurrentValue();

    const float sr = (float) sampleRate;

    // Sidechain filter: keeps the low end from pumping everything
    if (v[scHpf] != lastScHpf || sampleRate != lastRate)
    {
        scCoefs = KngEq::makeBiquad (KngEq::lowCut, v[scHpf], 0.0, 0.707, sampleRate);
        lastScHpf = v[scHpf];
        lastRate = sampleRate;
    }

    const int m = juce::roundToInt (v[mode]);
    float attackMs = v[attack];
    float releaseMs = v[release];

    if (m == punch)   releaseMs *= 0.6f;                      // snappier recovery
    if (m == opto)    attackMs = juce::jmax (attackMs, 10.0f); // never grabs instantly
    if (m == vintage) attackMs *= 0.25f;                       // fast grab

    const float aCoef = std::exp (-1.0f / (attackMs * 0.001f * sr));
    const float rCoefBase = std::exp (-1.0f / (releaseMs * 0.001f * sr));

    const float T = v[threshold], R = v[ratio], K = v[knee];
    const float makeupDb = v[makeup] + (v[autoMakeup] > 0.5f ? -gainComputer (0.0f, T, R, K) * 0.5f : 0.0f);
    const float mixAmount = v[mix];

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = chans > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;

    float inMax = 0.0f, outMax = 0.0f, grMax = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float l = left[i];
        const float r = right != nullptr ? right[i] : l;

        // Both channels feed one detector, so the stereo image never shifts
        const float dl = scState[0].process (l, scCoefs);
        const float dr = right != nullptr ? scState[1].process (r, scCoefs) : dl;
        const float detectDb = juce::Decibels::gainToDecibels (juce::jmax (std::abs (dl), std::abs (dr)), -120.0f);
        const float target = gainComputer (detectDb, T, R, K);

        // Opto: the harder it works, the slower it lets go
        const float rCoef = m == opto ? std::exp (-1.0f / (releaseMs * (1.0f - grDb / 6.0f) * 0.001f * sr)) : rCoefBase;

        grDb = target < grDb ? aCoef * grDb + (1.0f - aCoef) * target
                             : rCoef * grDb + (1.0f - rCoef) * target;

        const float g = juce::Decibels::decibelsToGain (grDb + makeupDb);
        float wl = l * g, wr = r * g;

        // Vintage: a little grit that grows as it compresses
        if (m == vintage)
        {
            const float drive = 1.0f - grDb * 0.05f;
            wl = std::tanh (wl * drive) / drive;
            wr = std::tanh (wr * drive) / drive;
        }

        const float ol = l + (wl - l) * mixAmount;
        const float orr = r + (wr - r) * mixAmount;

        left[i] = ol;

        if (right != nullptr)
            right[i] = orr;

        inMax  = juce::jmax (inMax, std::abs (l), std::abs (r));
        outMax = juce::jmax (outMax, std::abs (ol), std::abs (orr));
        grMax  = juce::jmax (grMax, -grDb);
    }

    raiseTo (inPeak, inMax);
    raiseTo (outPeak, outMax);
    raiseTo (grPeak, grMax);
}
