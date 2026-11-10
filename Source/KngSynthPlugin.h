#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "KngSynthParams.h"
#include "KngSynthDsp.h"

/** KNG Synth: two unison oscillators, sub, noise, filter, two envelopes, LFO, poly/mono/legato with glide. */
class KngSynthPlugin : public tracktion::Plugin
{
public:
    KngSynthPlugin (tracktion::PluginCreationInfo);
    ~KngSynthPlugin() override;

    static const char* getPluginName()     { return "KNG Synth"; }
    static const char* xmlTypeName;

    juce::String getName() const override                    { return getPluginName(); }
    juce::String getPluginType() override                    { return xmlTypeName; }
    juce::String getSelectableDescription() override         { return getPluginName(); }
    bool isSynth() override                                  { return true; }
    bool takesMidiInput() override                           { return true; }
    bool takesAudioInput() override                          { return false; }
    bool producesAudioWhenNoAudioInput() override            { return true; }
    int getNumOutputChannelsGivenInputs (int) override       { return 2; }

    // A synth: no audio coming in, stereo going out
    BusLayout getBusses() const override
    {
        BusLayout layout;
        layout.outputs.push_back (tracktion::ChannelConfiguration::stereo());
        return layout;
    }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    tracktion::AutomatableParameter* getParam (int index) const   { return params[(size_t) index].get(); }

    /** Message thread: load a factory preset. */
    void applyPreset (int presetIndex);

private:
    struct Block
    {
        float v[KngSynth::numParams];
    };

    void noteOn (int note, float velocity, const Block&);
    void noteOff (int note, const Block&);
    void removeHeld (int note);
    void render (juce::AudioBuffer<float>& out, int start, int num, const Block&);

    // Counted references keep each parameter alive until this plugin is done with it
    std::array<tracktion::AutomatableParameter::Ptr, KngSynth::numParams> params {};
    juce::OwnedArray<juce::CachedValue<float>> values;

    std::array<KngSynth::Voice, 16> voices;
    std::array<int, 128> held {};
    int heldCount = 0;
    juce::uint32 voiceCounter = 0;
    double lfoPhase = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngSynthPlugin)
};
