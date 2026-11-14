#pragma once

#include "KngFaceView.h"

class KngLoFiPlugin;

/** KNG Lo-Fi's face: burnt orange, a cassette whose reels spin while it plays and wobble with Wobble. */
class KngLoFiView : public KngFaceView
{
public:
    explicit KngLoFiView (KngLoFiPlugin& owner);
    ~KngLoFiView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    void paintReel (juce::Graphics&, juce::Point<float> centre, float radius, float angle, float tapeAmount);
    float value (int p) const;

    KngLoFiPlugin& fx;
    float inDb = -100.0f, outDb = -100.0f;
    float angle = 0.0f;
    double clock = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngLoFiView)
};
