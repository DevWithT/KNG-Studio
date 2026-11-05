#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "LatencyTester.h"

class LatencyDialog : public juce::Component,
                      private juce::Timer
{
public:
    LatencyDialog (EditSession& sessionToUse, juce::AudioDeviceManager& deviceManagerToUse);
    ~LatencyDialog() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void runTest();
    void stopTest();
    void finishTest();

    EditSession& session;
    juce::AudioDeviceManager& deviceManager;

    LatencyTester tester;
    bool running = false;
    bool monitoringWasOn = true;

    juce::Label infoLabel, resultLabel, offsetLabel { {}, "Offset (ms)" };
    juce::TextButton runButton { "Run Test" }, applyButton { "Apply Offset" };
    juce::Slider offsetSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LatencyDialog)
};
