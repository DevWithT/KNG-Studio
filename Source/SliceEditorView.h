#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "EditSession.h"

/** Waveform of a clip with slice markers. Sends the slices to Rack rows or to Drum Kit pads. */
class SliceEditorView : public juce::Component, private juce::Timer
{
public:
    SliceEditorView (EditSession& sessionToUse, tracktion::AudioClipBase& clipToUse);
    ~SliceEditorView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;

private:
    juce::Rectangle<int> getWaveArea() const;
    double xToTime (float x) const;
    float timeToX (double t) const;
    int markerAt (float x) const;
    std::vector<double> getFullCuts() const;
    void loadPeaks();
    void detect();
    void updateInfo();
    void gridCut (double beats);
    void splitCut (int parts);
    void chopToRack();
    void sendToPads();
    void close();
    void setupTap();
    void setupFx();
    void setupTempo();
    double guessSourceBpm() const;
    void layoutFxRow (juce::Rectangle<int> row);
    void drawSliceExtras (juce::Graphics& g, juce::Rectangle<int> wave) const;
    bool selectSliceAt (juce::Point<int> p);
    void selectSlice (int index);
    void syncSliceFx();
    void layoutTapRow (juce::Rectangle<int> row);
    void drawPlayhead (juce::Graphics& g, juce::Rectangle<int> wave) const;
    double playheadTime() const;
    void startPlay();
    void stopPlay();
    void tapCut();
    void timerCallback() override;

    EditSession& session;
    tracktion::AudioClipBase* clip = nullptr;   // the window is modal, so the clip stays put

    double clipStart = 0.0;
    double length = 1.0;
    std::vector<double> cuts;                   // seconds from the clip start, between the two ends
    int dragging = -1;

    bool playing = false;
    bool tapKeyHeld = false;
    double savedPosition = 0.0;                 // where the playhead was before Play, put back on Stop
    juce::TextButton playButton { "Play" }, tapButton { "Cut Here (T)" };
    juce::Label tapLabel;

    std::vector<SliceFx> sliceFx;               // one per slice, in order
    std::vector<double> prevCuts;               // the cuts at the last sync, to see which slice was split or merged
    int selected = 0;
    juce::TextButton prevSliceButton { "<" }, nextSliceButton { ">" };
    juce::Label sliceLabel, pitchLabel { {}, "Pitch" }, volLabel { {}, "Volume" };
    juce::ToggleButton reverseButton { "Reverse" };
    juce::Slider pitchSlider, gainSlider;
    juce::ToggleButton matchToggle { "Match tempo" };
    juce::Slider sourceBpm;
    juce::TextButton detectBpmButton { "Detect" };

    static constexpr int numPeaks = 2048;
    std::vector<float> peaksLo, peaksHi;
    float peakMax = 0.0f;

    juce::Label sensLabel { {}, "Sensitivity" }, infoLabel, hintLabel;
    juce::Slider sensitivity;
    juce::ComboBox gridBox, splitBox;
    juce::TextButton detectButton { "Detect Hits" }, clearButton { "Clear" },
                     rackButton { "Chop to Rack" }, padsButton { "Send to Pads..." }, doneButton { "Done" };

    static constexpr int toolbarHeight = 80;
    static constexpr int bottomHeight  = 84;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceEditorView)
};

//==============================================================================
/** Opens the Slice Editor in its own window for an audio clip. */
void showSliceEditor (EditSession& session, tracktion::AudioClipBase& clip);
