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
    void importBeat();
    void showExportDialog();
    static juce::String formatTime (double seconds);

    EditSession& session;

    // Row 1
    juce::TextButton playButton { "Play" }, stopButton { "Stop" }, recordButton { "Record" };
    juce::Label timeLabel;
    juce::ToggleButton monitorButton { "Monitor" }, clickButton { "Click" };
    juce::ComboBox countInBox;
    juce::Slider tempoSlider;

    // Row 2
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" }, importButton { "Import Beat" }, exportButton { "Export" };
    juce::ToggleButton loopButton { "Loop" }, snapButton { "Snap" }, writeButton { "Auto Write" };

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};
