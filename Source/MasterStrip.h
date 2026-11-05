#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "LevelMeter.h"

class MasterStrip : public juce::Component
{
public:
    explicit MasterStrip (EditSession& sessionToUse);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EditSession& session;

    juce::Label label;
    juce::Slider volumeSlider;
    juce::TextButton fxButton { "FX" };
    LevelMeter meter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterStrip)
};
