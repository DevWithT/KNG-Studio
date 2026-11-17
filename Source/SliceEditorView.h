#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "EditSession.h"

/** Waveform of a clip with slice markers. Sends the slices to Rack rows or to Drum Kit pads. */
class SliceEditorView : public juce::Component
{
public:
    SliceEditorView (EditSession& sessionToUse, tracktion::AudioClipBase& clipToUse);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> getWaveArea() const;
    double xToTime (float x) const;
    float timeToX (double t) const;
    int markerAt (float x) const;
    std::vector<double> getFullCuts() const;
    void loadPeaks();
    void detect();
    void updateInfo();
    void chopToRack();
    void sendToPads();
    void close();

    EditSession& session;
    tracktion::AudioClipBase* clip = nullptr;   // the window is modal, so the clip stays put

    double clipStart = 0.0;
    double length = 1.0;
    std::vector<double> cuts;                   // seconds from the clip start, between the two ends
    int dragging = -1;

    static constexpr int numPeaks = 2048;
    std::vector<float> peaksLo, peaksHi;
    float peakMax = 0.0f;

    juce::Label sensLabel { {}, "Sensitivity" }, infoLabel, hintLabel;
    juce::Slider sensitivity;
    juce::TextButton detectButton { "Detect Hits" }, clearButton { "Clear" },
                     rackButton { "Chop to Rack" }, padsButton { "Send to Pads..." }, doneButton { "Done" };

    static constexpr int toolbarHeight = 44;
    static constexpr int bottomHeight  = 44;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceEditorView)
};

//==============================================================================
/** Opens the Slice Editor in its own window for an audio clip. */
void showSliceEditor (EditSession& session, tracktion::AudioClipBase& clip);
