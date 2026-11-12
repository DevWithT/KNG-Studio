#pragma once

#include "KngFx.h"
#include <array>
#include <atomic>

/** KNG 808: a synthesized 808. Pitch drop, length, punch, drive, and glide for slides. One note at a time. */
class Kng808Plugin : public KngFxBase
{
public:
    enum Param { tune, drop, dropTime, length, hold, punch, drive, glide, level };

    Kng808Plugin (tracktion::PluginCreationInfo);
    ~Kng808Plugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override        { return "KNG 808"; }
    juce::String getPluginType() override        { return xmlTypeName; }
    juce::String getHint() const override        { return "Overlap two notes (or press G in the piano roll) to slide between them."; }

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
    void drawDisplay (juce::Graphics&, juce::Rectangle<float>, const KngHistory&) override;

    /** The KNG 808's own panel. */
    std::unique_ptr<EditorComponent> createEditor() override;

    /** The note sounding now, or -1. For the panel's screen and keys. */
    std::atomic<int> displayNote { -1 };

private:
    struct Voice
    {
        bool active = false, held = false, releasing = false, fast = false;
        int note = 0;
        float vel = 1.0f, env = 1.0f, rel = 1.0f;
        double phase = 0.0, pitch = 60.0, target = 60.0, t = 0.0;
    };

    struct Block
    {
        float tuneSt, dropSt, dropTauS, decayMul, punchAmt, driveAmt, glideCoef, gain;
        bool holdOn;
    };

    Block readBlock();
    void noteOn (int note, float velocity, const Block&);
    void noteOff (int note, const Block&);
    void render (juce::AudioBuffer<float>& out, int start, int num, const Block&);

    std::array<Voice, 2> voices;   // [0] plays, [1] fades out the note it replaced
    bool wasPlaying = false;
};
