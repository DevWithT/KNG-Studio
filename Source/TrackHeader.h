#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "LevelMeter.h"

class TrackHeader : public juce::Component,
                    private juce::Timer
{
public:
    TrackHeader (EditSession& sessionToUse, int trackIndexToUse);
    ~TrackHeader() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void refreshInputList();

    EditSession& session;
    const int trackIndex;
    const bool isBus;

    juce::Label nameLabel, sendLabel;
    juce::TextButton armButton { "R" }, muteButton { "M" }, soloButton { "S" }, fxButton { "FX" };
    juce::ComboBox inputBox;
    juce::Slider volumeSlider, panSlider, sendSlider;
    LevelMeter meter;

    juce::StringArray lastInputNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackHeader)
};
