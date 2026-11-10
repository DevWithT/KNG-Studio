#include "KngFx.h"
#include "KngFxView.h"

namespace te = tracktion;

static void raisePeak (std::atomic<float>& a, float v)
{
    float current = a.load();

    while (v > current && ! a.compare_exchange_weak (current, v)) {}
}

KngFxBase::KngFxBase (te::PluginCreationInfo creationInfo, std::vector<KngParamDef> paramDefs,
                      std::vector<KngPresetDef> presetDefs)
    : te::Plugin (creationInfo), defs (std::move (paramDefs)), presets (std::move (presetDefs))
{
    auto* um = getUndoManager();

    for (auto& d : defs)
    {
        juce::NormalisableRange<float> range (d.min, d.max, d.step);

        if (d.mid > d.min && d.mid < d.max)
            range.setSkewForCentre (d.mid);

        auto* value = values.add (new juce::CachedValue<float>());
        value->referTo (state, d.id, um, d.def);

        auto* param = addParam (d.id, d.name, range);
        param->attachToCurrentValue (*value);
        params.push_back (param);
    }
}

KngFxBase::~KngFxBase()
{
    for (auto& p : params)
        if (p != nullptr)
            p->detachFromCurrentValue();
}

std::unique_ptr<te::Plugin::EditorComponent> KngFxBase::createEditor()
{
    return std::make_unique<KngFxView> (*this);
}

void KngFxBase::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    for (int i = 0; i < (int) defs.size(); ++i)
    {
        const juce::Identifier id (defs[(size_t) i].id);

        if (v.hasProperty (id))
            values[i]->setValue ((float) v.getProperty (id), nullptr);
    }

    for (auto* p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

void KngFxBase::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    for (int i = 0; i < (int) defs.size(); ++i)
        params[(size_t) i]->setParameter (defs[(size_t) i].def, juce::sendNotification);

    for (auto& [p, value] : presets[(size_t) index].values)
        params[(size_t) p]->setParameter (value, juce::sendNotification);
}

void KngFxBase::takeMeters (float& in, float& out, float& reductionDb)
{
    in = inPeak.exchange (0.0f);
    out = outPeak.exchange (0.0f);
    reductionDb = grPeak.exchange (0.0f);
}

void KngFxBase::reportMeters (float in, float out, float reductionDb)
{
    raisePeak (inPeak, in);
    raisePeak (outPeak, out);
    raisePeak (grPeak, reductionDb);
}
