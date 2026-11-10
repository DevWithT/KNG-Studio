#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "KngCompDsp.h"
#include "KngEqDsp.h"
#include <atomic>

/** KNG Comp: four modes, soft knee, parallel mix, sidechain filter, metering. */
class KngCompPlugin : public tracktion::Plugin
{
public:
    KngCompPlugin (tracktion::PluginCreationInfo);
    ~KngCompPlugin() override;

    static const char* getPluginName()     { return "KNG Comp"; }
    static const char* xmlTypeName;

    juce::String getName() const override                    { return getPluginName(); }
    juce::String getPluginType() override                    { return xmlTypeName; }
    juce::String getSelectableDescription() override         { return getPluginName(); }
    BusLayout getBusses() const override                     { return BusLayout::singleStereoInOut(); }

    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override
    {
        getLeftRightChannelNames (ins, outs);
    }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;
    std::unique_ptr<EditorComponent> createEditor() override;

    tracktion::AutomatableParameter* getParam (int index) const   { return params[(size_t) index]; }
    void applyPreset (int presetIndex);

    /** Message thread: peaks since the last call (input and output as gain, reduction in dB). */
    void takeMeters (float& inputPeak, float& outputPeak, float& reductionDb);

private:
    std::array<tracktion::AutomatableParameter*, KngComp::numParams> params {};
    juce::OwnedArray<juce::CachedValue<float>> values;

    float grDb = 0.0f;
    KngEq::Coefs scCoefs;
    std::array<KngEq::State, 2> scState {};
    float lastScHpf = -1.0f;
    double lastRate = 0.0;

    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, grPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngCompPlugin)
};
