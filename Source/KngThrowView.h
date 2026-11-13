#pragma once

#include "KngFaceView.h"

class KngThrowPlugin;

/** KNG Throw's face: yellow, a trail of echoes bouncing away (side to side in Ping-Pong) that dims while it's ducking. */
class KngThrowView : public KngFaceView
{
public:
    explicit KngThrowView (KngThrowPlugin& owner);
    ~KngThrowView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    float value (int p) const;

    KngThrowPlugin& fx;
    float inDb = -100.0f, outDb = -100.0f, duckDb = 0.0f;
    double clock = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngThrowView)
};
