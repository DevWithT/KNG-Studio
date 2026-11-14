#pragma once

#include "KngFx.h"
#include <array>
#include <atomic>

/** KNG Keys: piano, Rhodes and organ, all synthesized (no samples). Plays chords. */
class KngKeysPlugin : public KngFxBase
{
public:
    enum Param { sound, tone, character, decay, motion, speed, width, velSens, level };

    KngKeysPlugin (tracktion::PluginCreationInfo);
    ~KngKeysPlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override        { return "KNG Keys"; }
    juce::String getPluginType() override        { return xmlTypeName; }
    juce::String getHint() const override        { return "Pick Piano, Rhodes or Organ up top. Character is the hammer, the bark or the drawbars."; }

    // An instrument: notes in, stereo out
    bool isSynth() override                                  { return true; }
    bool takesMidiInput() override                           { return true; }
    bool takesAudioInput() override                          { return false; }
    bool producesAudioWhenNoAudioInput() override            { return true; }
    int getNumOutputChannelsGivenInputs (int) override       { return 2; }

    BusLayout getBusses() const override
    {
        BusLayout layout;
        layout.outputs.push_back (tracktion::ChannelConfiguration::stereo());
        return layout;
    }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;

    /** This instrument's own face. */
    std::unique_ptr<EditorComponent> createEditor() override;

    static juce::StringArray soundNames()   { return { "Piano", "Rhodes", "Organ" }; }

    /** True while a note is sounding (for the face's keyboard). */
    bool isNoteLit (int note) const;

private:
    static constexpr int numVoices = 16;
    static constexpr int numPartials = 10;

    struct Voice
    {
        bool active = false, held = false;
        int note = 0, sound = 0;
        float gain = 1.0f, rel = 1.0f, pan = 0.0f;
        float bark = 0.0f, barkMul = 1.0f, click = 0.0f, clickMul = 1.0f, attack = 1.0f;
        int numUsed = 0;
        std::array<double, numPartials> phase {}, inc {};
        std::array<float, numPartials> amp {}, env {}, mul {};
    };

    void noteOn (int note, float velocity);
    void noteOff (int note);
    void render (juce::AudioBuffer<float>& out, int start, int num);
    void publishLitNotes();

    std::array<Voice, numVoices> voices;
    double sampleRate = 44100.0;
    double lfoPhase = 0.0;
    std::array<float, 2> lowState {};
    bool wasPlaying = false;
    juce::Random random;
    std::atomic<juce::uint64> litLow { 0 }, litHigh { 0 };
};
