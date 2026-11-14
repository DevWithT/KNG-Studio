#pragma once

#include "KngFx.h"
#include "KngFilters.h"
#include <array>
#include <vector>

/** KNG Lo-Fi: worn tape and old sampler grit. Fewer bits, lower rate, wobble, dust and a dull top. */
class KngLoFiPlugin : public KngFxBase
{
public:
    enum Param { bits, rate, wobble, dust, tone, mix };

    KngLoFiPlugin (tracktion::PluginCreationInfo);
    ~KngLoFiPlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override   { return "KNG Lo-Fi"; }
    juce::String getPluginType() override   { return xmlTypeName; }
    juce::String getHint() const override   { return "Wobble for warped tape, Dust for vinyl crackle, Bits and Rate for old-sampler crunch."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;

    /** This effect's own face. */
    std::unique_ptr<EditorComponent> createEditor() override;

private:
    float readDelay (int channel, double delaySamples) const;

    double sampleRate = 44100.0;
    std::array<std::vector<float>, 2> lines;
    int writePos = 0;
    double wowPhase = 0.0, flutterPhase = 0.0;
    std::array<float, 2> held {};
    double holdCount = 0.0;
    std::array<KngDsp::Biquad, 2> toneLp;
    float lastTone = -1.0f;
    double lastRate = 0.0;
    juce::Random random;
};
