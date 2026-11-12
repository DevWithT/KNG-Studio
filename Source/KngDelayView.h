#pragma once

#include "KngFaceView.h"

class KngDelayPlugin;

/** KNG Delay's face: teal, each echo a fading bar on the song's beat grid. */
class KngDelayView : public KngFaceView
{
public:
    explicit KngDelayView (KngDelayPlugin& owner);
    ~KngDelayView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    float value (int p) const;

    KngDelayPlugin& delay;
    KngFaceKnob* timeLKnob = nullptr;
    KngFaceKnob* timeRKnob = nullptr;
    KngFaceKnob* divLKnob = nullptr;
    KngFaceKnob* divRKnob = nullptr;
    float wetDb = -100.0f;
    double clock = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngDelayView)
};
