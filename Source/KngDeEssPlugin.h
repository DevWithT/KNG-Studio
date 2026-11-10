#pragma once

#include "KngFx.h"
#include "KngEqDsp.h"

/** KNG De-Esser: watches the hissy highs and turns them (or everything) down when they jump out. */
class KngDeEssPlugin : public KngFxBase
{
public:
    enum Param { freq, threshold, range, mode, listen };

    KngDeEssPlugin (tracktion::PluginCreationInfo);
    ~KngDeEssPlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override        { return "KNG De-Esser"; }
    juce::String getPluginType() override        { return xmlTypeName; }
    juce::String getHint() const override        { return "Tames harsh S and T sounds. Turn on Listen to hear what it catches."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;
    void drawDisplay (juce::Graphics&, juce::Rectangle<float>, const KngHistory&) override;

private:
    std::array<KngEq::Coefs, 4> detectCoefs {};
    KngEq::Coefs splitCoefs;
    std::array<std::array<KngEq::State, 2>, 2> detectState {};
    std::array<KngEq::State, 2> splitState {};
    float lastFreq = -1.0f;
    double lastRate = 0.0;
    float env = 0.0f;
};
