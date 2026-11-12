#pragma once

#include "KngFaceView.h"

class KngCompPlugin;

/** KNG Comp's face: amber and analog, a VU needle for gain reduction beside the compression curve. */
class KngCompView : public KngFaceView
{
public:
    explicit KngCompView (KngCompPlugin& owner);
    ~KngCompView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    void paintVu (juce::Graphics&, juce::Rectangle<float> area);
    void paintTransfer (juce::Graphics&, juce::Rectangle<float> area);
    float value (int p) const;

    KngCompPlugin& comp;
    float inDb = -100.0f, outDb = -100.0f, grNow = 0.0f, needle = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngCompView)
};
