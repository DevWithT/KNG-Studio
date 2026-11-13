#pragma once

#include "KngFaceView.h"

class KngRadioPlugin;

/** KNG Radio's face: brass and walnut, an old tuning dial with the band lit up and a speaker grille that pulses. */
class KngRadioView : public KngFaceView
{
public:
    explicit KngRadioView (KngRadioPlugin& owner);
    ~KngRadioView() override = default;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintSide (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    void paintGrille (juce::Graphics&, juce::Rectangle<float> area);
    void paintDial (juce::Graphics&, juce::Rectangle<float> area);

    KngRadioPlugin& fx;
    float inDb = -100.0f, outDb = -100.0f, pulse = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngRadioView)
};
