#pragma once

#include "KngFaceView.h"

class KngModPlugin;

/** KNG Mod's face: lavender, two waves (left and right) swaying with the sweep. */
class KngModView : public KngFaceView
{
public:
    explicit KngModView (KngModPlugin& owner);
    ~KngModView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    float value (int p) const;

    KngModPlugin& fx;
    float inDb = -100.0f, outDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngModView)
};
