#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

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

    juce::Label nameLabel;
    juce::TextButton armButton { "R" }, muteButton { "M" }, soloButton { "S" };
    juce::ComboBox inputBox;
    juce::Slider volumeSlider, panSlider;

    juce::StringArray lastInputNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackHeader)
};
