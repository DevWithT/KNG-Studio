#pragma once

#include "KngFaceView.h"

class KngVerbPlugin;

/** KNG Verb's face: deep blue, rings that grow with Size and fade with Decay. */
class KngVerbView : public KngFaceView
{
public:
    explicit KngVerbView (KngVerbPlugin& owner);
    ~KngVerbView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    float value (int p) const;

    KngVerbPlugin& verb;
    float wetDb = -100.0f, phase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngVerbView)
};
