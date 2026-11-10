#pragma once

#include "KngFx.h"

/** KNG Limiter: looks ahead so peaks are caught smoothly, with a ceiling that's never crossed. */
class KngLimiterPlugin : public KngFxBase
{
public:
    enum Param { gain, ceiling, release, style };
    enum Style { transparent, punchy, loud };

    KngLimiterPlugin (tracktion::PluginCreationInfo);
    ~KngLimiterPlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override        { return "KNG Limiter"; }
    juce::String getPluginType() override        { return xmlTypeName; }
    juce::String getHint() const override        { return "Push Gain for loudness. Nothing ever goes past the Ceiling."; }
    double getLatencySeconds() override          { return lookahead / juce::jmax (1.0, sampleRate); }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;
    void drawDisplay (juce::Graphics&, juce::Rectangle<float>, const KngHistory&) override;

private:
    int lookahead = 72;
    int pos = 0;
    std::array<std::vector<float>, 2> delayBuf;
    std::vector<float> holdBuf, boxBuf;
    double boxSum = 0.0;
    float releaseEnv = 1.0f;
};
