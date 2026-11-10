#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include <juce_dsp/juce_dsp.h>
#include "KngSatDsp.h"
#include "KngEqDsp.h"
#include <atomic>

/** KNG Saturator: four flavors, 4x oversampled, tilt tone, real loudness matching. */
class KngSatPlugin : public tracktion::Plugin
{
public:
    KngSatPlugin (tracktion::PluginCreationInfo);
    ~KngSatPlugin() override;

    static const char* getPluginName()     { return "KNG Saturator"; }
    static const char* xmlTypeName;

    juce::String getName() const override                    { return getPluginName(); }
    juce::String getPluginType() override                    { return xmlTypeName; }
    juce::String getSelectableDescription() override         { return getPluginName(); }
    BusLayout getBusses() const override                     { return BusLayout::singleStereoInOut(); }
    double getLatencySeconds() override                      { return latencySamples / juce::jmax (1.0, sampleRate); }

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

    tracktion::AutomatableParameter* getParam (int index) const   { return params[(size_t) index].get(); }
    void applyPreset (int presetIndex);

    /** Message thread: peaks since the last call, as gain. */
    void takeMeters (float& inputPeak, float& outputPeak);

private:
    static constexpr int chunk = 512;

    // Counted references keep each parameter alive until this plugin is done with it
    std::array<tracktion::AutomatableParameter::Ptr, KngSat::numParams> params {};
    juce::OwnedArray<juce::CachedValue<float>> values;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int latencySamples = 0;

    // The dry signal, delayed to line up with the oversampled wet signal
    std::array<std::vector<float>, 2> dryDelay;
    int dryPos = 0;
    std::array<std::array<float, chunk>, 2> dryHold {};

    std::array<KngEq::State, 2> lowShelfState {}, highShelfState {};
    KngEq::Coefs lowShelfCoefs, highShelfCoefs;
    float lastTone = -99.0f;
    double lastRate = 0.0;

    std::array<float, 2> dcX {}, dcY {};
    float inPower = 0.0f, outPower = 0.0f, matchGain = 1.0f;

    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngSatPlugin)
};
