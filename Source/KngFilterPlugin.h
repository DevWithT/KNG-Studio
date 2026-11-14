#pragma once

#include "KngFx.h"
#include <array>
#include <atomic>
#include <functional>

/** KNG Filter: a resonant sweep filter for beats and instruments, with a tempo-synced wobble. */
class KngFilterPlugin : public KngFxBase
{
public:
    enum Param { mode, cutoff, reso, drive, lfoRate, lfoDepth, mix };

    KngFilterPlugin (tracktion::PluginCreationInfo);
    ~KngFilterPlugin() override;

    static const char* xmlTypeName;
    juce::String getName() const override   { return "KNG Filter"; }
    juce::String getPluginType() override   { return xmlTypeName; }
    juce::String getHint() const override   { return "Sweep Cutoff for build-ups and drops. Pick a Wobble speed to move it with the beat."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;

    /** This effect's own face. */
    std::unique_ptr<EditorComponent> createEditor() override;

    /** Where the cutoff is right now, wobble included (for the face). */
    float getLiveCutoff() const   { return liveCutoff.load(); }

    /** The filter's response in dB at a frequency, for a given cutoff (for the face's curve). */
    float responseAt (float hz, float cutoffHz) const;

private:
    struct BpmTimer : public juce::Timer
    {
        std::function<void()> onTick;
        void timerCallback() override   { if (onTick) onTick(); }
    };

    double sampleRate = 44100.0;
    std::array<float, 2> ic1 {}, ic2 {};
    double lfoPhase = 0.0;
    std::atomic<float> liveCutoff { 1000.0f };
    std::atomic<float> bpm { 120.0f };
    BpmTimer bpmTimer;
};
