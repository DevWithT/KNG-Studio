#pragma once

#include "KngFaceView.h"

class KngFilterPlugin;

/** KNG Filter's face: hot pink, the filter curve riding the wobble in real time. */
class KngFilterView : public KngFaceView
{
public:
    explicit KngFilterView (KngFilterPlugin& owner);
    ~KngFilterView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    KngFilterPlugin& fx;
    float inDb = -100.0f, outDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngFilterView)
};
