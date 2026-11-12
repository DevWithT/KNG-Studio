#pragma once

#include "KngFaceView.h"

class KngLimiterPlugin;

/** KNG Limiter's face: rose red, the ceiling line over a reduction history, and a big output readout. */
class KngLimiterView : public KngFaceView
{
public:
    explicit KngLimiterView (KngLimiterPlugin& owner);
    ~KngLimiterView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    KngLimiterPlugin& fx;
    float inDb = -100.0f, outDb = -100.0f, grNow = 0.0f, outHold = -100.0f;
    int holdTicks = 0;
    std::vector<float> grHistory = std::vector<float> (240, 0.0f), outHistory = std::vector<float> (240, -100.0f);
    int historyPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngLimiterView)
};
