#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

class TransportBar : public juce::Component,
                     private juce::Timer
{
public:
    explicit TransportBar (EditSession& sessionToUse);
    ~TransportBar() override;

    void resized() override;

private:
    void timerCallback() override;
    static juce::String formatTime (double seconds);

    EditSession& session;

    juce::TextButton playButton { "Play" }, stopButton { "Stop" }, recordButton { "Record" };
    juce::ToggleButton armButton { "Arm Track" };
    juce::Label timeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};
