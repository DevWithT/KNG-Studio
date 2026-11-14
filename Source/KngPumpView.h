#pragma once

#include "KngFaceView.h"

class KngPumpPlugin;

/** KNG Pump's face: emerald, the volume curve for two cycles with a dot riding it in time. */
class KngPumpView : public KngFaceView
{
public:
    explicit KngPumpView (KngPumpPlugin& owner);
    ~KngPumpView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    KngPumpPlugin& fx;
    float inDb = -100.0f, outDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngPumpView)
};
