#pragma once

#include "KngFaceView.h"

class KngWidthPlugin;

/** KNG Width's face: indigo, a stereo scope that spreads as you widen, with a mono-safety meter under it. */
class KngWidthView : public KngFaceView
{
public:
    explicit KngWidthView (KngWidthPlugin& owner);
    ~KngWidthView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    KngWidthPlugin& fx;
    float inDb = -100.0f, outDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngWidthView)
};
