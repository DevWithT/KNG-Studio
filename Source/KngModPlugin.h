#pragma once

#include "KngFx.h"
#include <array>
#include <atomic>
#include <vector>

/** KNG Mod: chorus, flanger and phaser in one, for keys, guitars, pads and leads. */
class KngModPlugin : public KngFxBase
{
public:
    enum Param { mode, rate, depth, feedback, width, mix };

    KngModPlugin (tracktion::PluginCreationInfo);
    ~KngModPlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override   { return "KNG Mod"; }
    juce::String getPluginType() override   { return xmlTypeName; }
    juce::String getHint() const override   { return "Chorus thickens, Flanger swooshes, Phaser swirls. Width spreads the movement left and right."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;

    /** This effect's own face. */
    std::unique_ptr<EditorComponent> createEditor() override;

    /** Where the sweep is right now, 0 to 1 (for the face). */
    double getPhase() const   { return phaseNow.load(); }

private:
    float readDelay (int channel, double delaySamples) const;

    double sampleRate = 44100.0;
    std::array<std::vector<float>, 2> lines;
    int writePos = 0;
    double phase = 0.0;
    std::array<float, 2> fbState {};
    std::array<std::array<float, 6>, 2> apState {};
    std::atomic<double> phaseNow { 0.0 };
};
