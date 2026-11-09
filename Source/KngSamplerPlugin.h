#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include <array>

/** KNG Sampler: one sample across the keyboard, with sustain loop, glide and mono cut. */
class KngSamplerPlugin : public tracktion::Plugin
{
public:
    KngSamplerPlugin (tracktion::PluginCreationInfo);
    ~KngSamplerPlugin() override;

    static const char* getPluginName()     { return "KNG Sampler"; }
    static const char* xmlTypeName;

    juce::String getName() const override                    { return getPluginName(); }
    juce::String getPluginType() override                    { return xmlTypeName; }
    bool isSynth() override                                  { return true; }
    bool takesMidiInput() override                           { return true; }
    bool takesAudioInput() override                          { return false; }
    bool producesAudioWhenNoAudioInput() override            { return true; }
    int getNumOutputChannelsGivenInputs (int) override       { return 2; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    // Message thread only
    bool loadSample (const juce::File& file);
    juce::File getSampleFile() const;
    void syncFromState();
    std::shared_ptr<juce::AudioBuffer<float>> getSampleForDisplay();

    // Settings, stored in the plugin's state
    static inline const juce::Identifier fileProp      { "kngFile" };
    static inline const juce::Identifier rootProp      { "kngRoot" };
    static inline const juce::Identifier gainProp      { "kngGain" };
    static inline const juce::Identifier attackProp    { "kngAttack" };
    static inline const juce::Identifier releaseProp   { "kngRelease" };
    static inline const juce::Identifier glideProp     { "kngGlide" };
    static inline const juce::Identifier monoProp      { "kngMono" };
    static inline const juce::Identifier oneShotProp   { "kngOneShot" };
    static inline const juce::Identifier loopProp      { "kngLoop" };
    static inline const juce::Identifier loopStartProp { "kngLoopStart" };
    static inline const juce::Identifier loopEndProp   { "kngLoopEnd" };

private:
    struct Settings
    {
        int root = 60;
        float gainDb = 0.0f, attackMs = 2.0f, releaseMs = 30.0f, glideMs = 0.0f;
        bool mono = false, oneShot = false, loop = false;
        float loopStart = 0.4f, loopEnd = 0.9f;
    };

    struct Voice
    {
        bool active = false, releasing = false, fast = false;
        int note = 0;
        double pos = 0.0, inc = 1.0, targetInc = 1.0;
        float env = 0.0f, vel = 1.0f;
    };

    void readFile (const juce::File& file);
    void noteOn (int note, float velocity, const Settings& s, double fileRate);
    void noteOff (int note, const Settings& s);
    void renderVoices (juce::AudioBuffer<float>& out, int start, int num, const Settings& s,
                       const juce::AudioBuffer<float>& smp, double fileRate);

    juce::SpinLock lock;
    Settings settings;
    std::shared_ptr<juce::AudioBuffer<float>> sample;
    double sampleFileRate = 44100.0;
    juce::File loadedFile;

    double sampleRate = 44100.0;
    std::array<Voice, 16> voices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngSamplerPlugin)
};
