#pragma once

#include "KngFaceView.h"

class KngKeysPlugin;

/** KNG Keys' face: warm wood and brass, the sound's name up top and a keyboard that lights up as you play. */
class KngKeysView : public KngFaceView
{
public:
    explicit KngKeysView (KngKeysPlugin& owner);
    ~KngKeysView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    KngKeysPlugin& fx;
    float outDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngKeysView)
};
