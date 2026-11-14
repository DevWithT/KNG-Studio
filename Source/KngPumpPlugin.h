#pragma once

#include "KngFx.h"
#include <atomic>
#include <functional>

/** KNG Pump: sidechain-style volume pumping locked to the beat, no kick track needed. */
class KngPumpPlugin : public KngFxBase
{
public:
    enum Param { rate, depth, recovery, curve, offset };

    KngPumpPlugin (tracktion::PluginCreationInfo);
    ~KngPumpPlugin() override;

    static const char* xmlTypeName;
    juce::String getName() const override   { return "KNG Pump"; }
    juce::String getPluginType() override   { return xmlTypeName; }
    juce::String getHint() const override   { return "Ducks the sound on every beat like a kick sidechain. Put it on pads, chords or the 808."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;

    /** This effect's own face. */
    std::unique_ptr<EditorComponent> createEditor() override;

    static juce::StringArray rateNames()   { return { "1 Bar", "1/2", "1/4", "1/8", "1/16" }; }

    /** The volume (0 to 1) at a point in the cycle (0 to 1), for the face's curve. */
    float shapeAt (float cyclePos) const;

    /** Where in the cycle it is right now, 0 to 1. */
    float getCyclePos() const   { return cycleNow.load(); }

private:
    struct BpmTimer : public juce::Timer
    {
        std::function<void()> onTick;
        void timerCallback() override   { if (onTick) onTick(); }
    };

    double sampleRate = 44100.0;
    double freePhase = 0.0;
    float smoothGain = 1.0f;
    std::atomic<float> cycleNow { 0.0f };
    std::atomic<float> bpm { 120.0f };
    BpmTimer bpmTimer;
};
