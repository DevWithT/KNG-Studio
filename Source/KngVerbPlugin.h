#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "KngVerbDsp.h"
#include "KngEqDsp.h"
#include <atomic>

/** KNG Verb: 8-line feedback delay network with diffusion, modulation, damping, wet EQ and ducking. */
class KngVerbPlugin : public tracktion::Plugin
{
public:
    KngVerbPlugin (tracktion::PluginCreationInfo);
    ~KngVerbPlugin() override;

    static const char* getPluginName()     { return "KNG Verb"; }
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

    /** Message thread: the loudest reverb output since the last call (as gain). */
    float takeWetPeak()   { return wetPeak.exchange (0.0f); }

private:
    struct Line
    {
        std::vector<float> buf;
        int write = 0;
        float damp = 0.0f;
        double lfoPhase = 0.0;
    };

    struct Allpass
    {
        std::vector<float> buf;
        int pos = 0;
    };

    // Counted references keep each parameter alive until this plugin is done with it
    std::array<tracktion::AutomatableParameter::Ptr, KngVerb::numParams> params {};
    juce::OwnedArray<juce::CachedValue<float>> values;

    std::array<Line, 8> lines;
    std::array<Allpass, 4> diffusers;
    std::vector<float> preBuf;
    int preWrite = 0;

    std::array<KngEq::State, 2> lowCutState {}, highCutState {};
    KngEq::Coefs lowCutCoefs, highCutCoefs;
    float lastLowCut = -1.0f, lastHighCut = -1.0f;
    double lastRate = 0.0;

    float duckEnv = 0.0f;
    std::atomic<float> wetPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngVerbPlugin)
};
