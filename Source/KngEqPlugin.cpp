#include "KngEqPlugin.h"
#include "KngEqView.h"

namespace te = tracktion;

const char* KngEqPlugin::xmlTypeName = "kngeq";

KngEqPlugin::KngEqPlugin (te::PluginCreationInfo creationInfo)
    : te::Plugin (creationInfo)
{
    auto* um = getUndoManager();
    const auto& defs = KngEq::params();

    for (int i = 0; i < KngEq::numParams; ++i)
    {
        const auto& p = defs[(size_t) i];
        juce::NormalisableRange<float> range (p.min, p.max, p.step);

        if (p.mid > p.min && p.mid < p.max)
            range.setSkewForCentre (p.mid);

        auto* value = values.add (new juce::CachedValue<float>());
        value->referTo (state, juce::Identifier (p.id), um, p.def);

        auto* param = addParam (p.id, p.name, range);
        param->attachToCurrentValue (*value);
        params[(size_t) i] = param;
    }
}

KngEqPlugin::~KngEqPlugin()
{
    notifyListenersOfDeletion();

    for (auto* p : params)
        if (p != nullptr)
            p->detachFromCurrentValue();
}

std::unique_ptr<te::Plugin::EditorComponent> KngEqPlugin::createEditor()
{
    return std::make_unique<KngEqView> (*this);
}

void KngEqPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    lastRate = 0.0;   // forces a coefficient update
    reset();
}

void KngEqPlugin::reset()
{
    for (auto& channel : states)
        for (auto& band : channel)
            for (auto& s : band)
                s.reset();
}

void KngEqPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    const auto& defs = KngEq::params();

    for (int i = 0; i < KngEq::numParams; ++i)
    {
        const juce::Identifier id (defs[(size_t) i].id);

        if (v.hasProperty (id))
            values[i]->setValue ((float) v.getProperty (id), nullptr);
    }

    for (auto* p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

void KngEqPlugin::updateCoefficients (const float* v)
{
    using namespace KngEq;

    for (int b = 0; b < numBands; ++b)
    {
        // A band that just turned on starts from silence, not old filter state
        if (v[index (b, bandOn)] > 0.5f && lastValues[(size_t) index (b, bandOn)] < 0.5f)
            for (auto& channel : states)
                for (auto& s : channel[(size_t) b])
                    s.reset();

        stages[(size_t) b] = makeBand (juce::roundToInt (v[index (b, bandType)]),
                                       juce::roundToInt (v[index (b, bandSlope)]),
                                       v[index (b, bandFreq)], v[index (b, bandGain)], v[index (b, bandQ)],
                                       sampleRate, coefs[(size_t) b]);
    }

    for (int i = 0; i < numParams; ++i)
        lastValues[(size_t) i] = v[i];

    lastRate = sampleRate;
}

void KngEqPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    using namespace KngEq;

    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    float v[numParams];
    bool changed = sampleRate != lastRate;

    for (int i = 0; i < numParams; ++i)
    {
        v[i] = params[(size_t) i]->getCurrentValue();
        changed = changed || v[i] != lastValues[(size_t) i];
    }

    if (changed)
        updateCoefficients (v);

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    const int start = fc.bufferStartSample;
    const int num = fc.bufferNumSamples;
    const float outGain = juce::Decibels::decibelsToGain (v[outputParam]);

    for (int ch = 0; ch < chans; ++ch)
    {
        auto* d = buffer.getWritePointer (ch, start);

        for (int b = 0; b < numBands; ++b)
        {
            if (v[index (b, bandOn)] < 0.5f)
                continue;

            for (int s = 0; s < stages[(size_t) b]; ++s)
            {
                auto& st = states[(size_t) ch][(size_t) b][(size_t) s];
                const auto& c = coefs[(size_t) b][(size_t) s];

                for (int i = 0; i < num; ++i)
                    d[i] = st.process (d[i], c);
            }
        }

        buffer.applyGain (ch, start, num, outGain);
    }

    // Feed the analyzer a mono mix of what comes out
    int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
    fifo.prepareToWrite (num, s1, n1, s2, n2);

    auto monoAt = [&] (int i)
    {
        const float l = buffer.getSample (0, start + i);
        return chans > 1 ? 0.5f * (l + buffer.getSample (1, start + i)) : l;
    };

    for (int i = 0; i < n1; ++i)
        fifoData[(size_t) (s1 + i)] = monoAt (i);

    for (int i = 0; i < n2; ++i)
        fifoData[(size_t) (s2 + i)] = monoAt (n1 + i);

    fifo.finishedWrite (n1 + n2);
}

int KngEqPlugin::pullAnalyserSamples (float* dest, int maxSamples)
{
    int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
    fifo.prepareToRead (maxSamples, s1, n1, s2, n2);

    for (int i = 0; i < n1; ++i)
        dest[i] = fifoData[(size_t) (s1 + i)];

    for (int i = 0; i < n2; ++i)
        dest[n1 + i] = fifoData[(size_t) (s2 + i)];

    fifo.finishedRead (n1 + n2);
    return n1 + n2;
}
