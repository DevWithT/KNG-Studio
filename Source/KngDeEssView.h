#pragma once

#include "KngFaceView.h"

class KngDeEssPlugin;

/** KNG De-Esser's face: magenta, the "S" band lit on a frequency strip, with a reduction history. */
class KngDeEssView : public KngFaceView
{
public:
    explicit KngDeEssView (KngDeEssPlugin& owner);
    ~KngDeEssView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    KngDeEssPlugin& fx;
    float inDb = -100.0f, outDb = -100.0f, grNow = 0.0f;
    std::vector<float> grHistory = std::vector<float> (240, 0.0f), inHistory = std::vector<float> (240, -100.0f);
    int historyPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngDeEssView)
};
