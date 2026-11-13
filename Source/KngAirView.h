#pragma once

#include "KngFaceView.h"

class KngAirPlugin;

/** KNG Air's face: ice white, the lift curve over the highs with sparkles that twinkle with Shine. */
class KngAirView : public KngFaceView
{
public:
    explicit KngAirView (KngAirPlugin& owner);
    ~KngAirView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    float value (int p) const;

    KngAirPlugin& fx;
    float inDb = -100.0f, outDb = -100.0f;
    double clock = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngAirView)
};
