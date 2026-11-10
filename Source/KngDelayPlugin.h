#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "KngDelayDsp.h"
#include "KngEqDsp.h"
#include <atomic>

/** KNG Delay: tempo-synced stereo / ping-pong / mono delay with tape drive, wobble, filters and ducking. */
class KngDelayPlugin : public tracktion::Plugin,
                       private juce::Timer
{
public:
    KngDelayPlugin (tracktion::PluginCreationInfo);
    ~KngDelayPlugin() override;

    static const char* getPluginName()     { return "KNG Delay"; }
    static const char* xmlTypeName;

    juce::String getName() const override                    { return getPluginName(); }
    juce::String getPluginType() override                    { return xmlTypeName; }
    juce::String getSelectableDescription() override         { return getPluginName(); }
    BusLayout getBusses() const override                     { return BusLayout::singleStereoInOut(); }
    bool noTail() override                                   { return false; }
    double getTailLength() const override                    { return 20.0; }

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

    double getBpm() const                 { return bpm.load(); }
    double getDelaySeconds (int side) const;
    float takeWetPeak()                   { return wetPeak.exchange (0.0f); }

private:
    void timerCallback() override;

    // Counted references keep each parameter alive until this plugin is done with it
    std::array<tracktion::AutomatableParameter::Ptr, KngDelay::numParams> params {};
    juce::OwnedArray<juce::CachedValue<float>> values;

    std::vector<float> bufL, bufR;
    int writePos = 0;
    double smoothL = 0.0, smoothR = 0.0;
    double wowPhase = 0.0, flutterPhase = 0.0;

    std::array<KngEq::State, 2> lowCutState {}, highCutState {};
    KngEq::Coefs lowCutCoefs, highCutCoefs;
    float lastLowCut = -1.0f, lastHighCut = -1.0f;
    double lastRate = 0.0;

    float duckEnv = 0.0f;
    std::atomic<double> bpm { 120.0 };
    std::atomic<float> wetPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngDelayPlugin)
};
