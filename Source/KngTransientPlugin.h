#pragma once

#include "KngFx.h"
#include "KngFilters.h"

/** KNG Transient: more or less punch at the start of each hit, and more or less ring after it. */
class KngTransientPlugin : public KngFxBase
{
public:
    enum Param { attack, sustain, output, mix };

    KngTransientPlugin (tracktion::PluginCreationInfo);
    ~KngTransientPlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override   { return "KNG Transient"; }
    juce::String getPluginType() override   { return xmlTypeName; }
    juce::String getHint() const override   { return "Attack up for snappier drums, Sustain down for tighter ones. Works on any track, no threshold to set."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;

    /** This effect's own face. */
    std::unique_ptr<EditorComponent> createEditor() override;

    /** How a sample drum hit's loudness would change at a point (0 to 1 across the hit), in dB (for the face). */
    float exampleGainDb (float t) const;

private:
    double sampleRate = 44100.0;
    KngDsp::Follower fastEnv, slowEnv, holdEnv, dropEnv;
    float gainDb = 0.0f;
};
