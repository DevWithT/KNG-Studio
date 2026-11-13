#pragma once

#include "KngFaceView.h"

class KngGatePlugin;

/** KNG Gate's face: lime, the input level scrolling under the threshold line, with a strip showing when it's open. */
class KngGateView : public KngFaceView
{
public:
    explicit KngGateView (KngGatePlugin& owner);
    ~KngGateView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    KngGatePlugin& fx;
    float inDb = -100.0f, outDb = -100.0f;
    std::vector<float> inHistory = std::vector<float> (240, -100.0f), grHistory = std::vector<float> (240, 0.0f);
    int historyPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngGateView)
};
