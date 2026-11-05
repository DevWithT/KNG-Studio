#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

class LevelMeter : public juce::Component,
                   private juce::Timer
{
public:
    LevelMeter();
    ~LevelMeter() override;

    void attachTo (tracktion::Plugin* levelMeterPlugin);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void detach();

    tracktion::Plugin::Ptr owner;   // keeps the measurer alive
    tracktion::LevelMeasurer* measurer = nullptr;
    tracktion::LevelMeasurer::Client client;
    float levels[2] { -100.0f, -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};
