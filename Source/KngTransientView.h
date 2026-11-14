#pragma once

#include "KngFaceView.h"

class KngTransientPlugin;

/** KNG Transient's face: teal, a sample drum hit reshaped by Attack and Sustain, over a live history of what it's doing. */
class KngTransientView : public KngFaceView
{
public:
    explicit KngTransientView (KngTransientPlugin& owner);
    ~KngTransientView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    KngTransientPlugin& fx;
    float inDb = -100.0f, outDb = -100.0f;
    std::vector<float> moveHistory = std::vector<float> (180, 0.0f);
    int historyPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngTransientView)
};
