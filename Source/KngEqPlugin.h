#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "KngEqDsp.h"

/** KNG EQ: 8 bands, cuts up to 48 dB/oct, live analyzer. */
class KngEqPlugin : public tracktion::Plugin
{
public:
    KngEqPlugin (tracktion::PluginCreationInfo);
    ~KngEqPlugin() override;

    static const char* getPluginName()     { return "KNG EQ"; }
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

    tracktion::AutomatableParameter* getParam (int index) const   { return params[(size_t) index].get(); }
    double getSampleRate() const                                  { return sampleRate; }

    /** Message thread: takes the newest output samples for the analyzer. Returns how many. */
    int pullAnalyserSamples (float* dest, int maxSamples);

private:
    void updateCoefficients (const float* v);

    // Counted references keep each parameter alive until this plugin is done with it
    std::array<tracktion::AutomatableParameter::Ptr, KngEq::numParams> params {};
    juce::OwnedArray<juce::CachedValue<float>> values;

    std::array<std::array<KngEq::Coefs, 4>, KngEq::numBands> coefs {};
    std::array<int, KngEq::numBands> stages {};
    std::array<std::array<std::array<KngEq::State, 4>, KngEq::numBands>, 2> states {};
    std::array<float, KngEq::numParams> lastValues {};
    double lastRate = 0.0;

    juce::AbstractFifo fifo { 8192 };
    std::vector<float> fifoData = std::vector<float> (8192, 0.0f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngEqPlugin)
};
