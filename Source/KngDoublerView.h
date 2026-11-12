#pragma once

#include "KngFaceView.h"

class KngDoublerPlugin;

/** KNG Doubler's face: sky blue, your vocal in the middle of the stereo field with the doubles drifting around it. */
class KngDoublerView : public KngFaceView
{
public:
    explicit KngDoublerView (KngDoublerPlugin& owner);
    ~KngDoublerView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    float value (int p) const;

    KngDoublerPlugin& fx;
    float inDb = -100.0f, outDb = -100.0f;
    double clock = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngDoublerView)
};
