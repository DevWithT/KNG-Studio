#pragma once

#include "KngFaceView.h"

class KngSatPlugin;

/** KNG Saturator's face: burnt orange, a glowing tube beside the drive curve. */
class KngSatView : public KngFaceView
{
public:
    explicit KngSatView (KngSatPlugin& owner);
    ~KngSatView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    void paintTube (juce::Graphics&, juce::Rectangle<float> area);
    void paintCurve (juce::Graphics&, juce::Rectangle<float> area);
    float value (int p) const;

    KngSatPlugin& sat;
    float inDb = -100.0f, outDb = -100.0f, glow = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngSatView)
};
