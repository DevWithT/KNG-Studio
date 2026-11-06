#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

class TransportBar : public juce::Component,
                     private juce::Timer
{
public:
    explicit TransportBar (EditSession& sessionToUse);
    ~TransportBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Called from the File menu
    void importBeat();
    void showExportDialog();

private:
    void timerCallback() override;
    static juce::String formatTime (double seconds);

    EditSession& session;

    juce::TextButton playButton { "Play" }, stopButton { "Stop" }, recordButton { "Rec" };
    juce::Label timeLabel;
    juce::Slider tempoSlider;
    juce::ComboBox countInBox;
    juce::ToggleButton clickButton { "Click" }, loopButton { "Loop" }, punchButton { "Punch" }, snapButton { "Snap" },
                       monitorButton { "Monitor" }, writeButton { "Auto Write" };

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<int> dividers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};
