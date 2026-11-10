#pragma once

#include "KngFx.h"
#include "KngEqDsp.h"

/** KNG Doubler: one vocal becomes several takes, spread across the stereo field. */
class KngDoublerPlugin : public KngFxBase
{
public:
    enum Param { detune, delay, voices, width, lowCut, mix };

    KngDoublerPlugin (tracktion::PluginCreationInfo);
    ~KngDoublerPlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override        { return "KNG Doubler"; }
    juce::String getPluginType() override        { return xmlTypeName; }
    juce::String getHint() const override        { return "Makes one take sound like several. Great on hooks and ad-libs."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;
    void drawDisplay (juce::Graphics&, juce::Rectangle<float>, const KngHistory&) override;

private:
    std::vector<float> buf;
    int writePos = 0;
    std::array<double, 4> phases {};
    std::array<KngEq::State, 4> lowCutState {};
    KngEq::Coefs lowCutCoefs;
    float lastLowCut = -1.0f;
    double lastRate = 0.0;
};
