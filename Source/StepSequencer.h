#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** FL-style step grid: one row per drum pad, 16th-note steps. */
class StepSequencer : public juce::Component,
                      private juce::Timer
{
public:
    StepSequencer (EditSession& sessionToUse, tracktion::Track& trackToUse);
    ~StepSequencer() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    int getTrackIndex();
    tracktion::MidiClip* findPatternClip();
    void syncControls();
    juce::UndoManager* um();

    // Pattern timing
    double getClipStartBeat() const;
    double getClipLengthBeats() const;
    int getNumSteps() const;
    double getSwingDelay() const;
    double stepToBeat (int step) const;
    int beatToStep (double beatFromClipStart) const;

    // Grid
    juce::Rectangle<int> getGridArea() const;
    juce::Rectangle<int> getCellBounds (int row, int step) const;
    bool cellAt (juce::Point<int>, int& row, int& step) const;
    tracktion::MidiNote* findNote (int row, int step);
    void setStep (int row, int step, bool on);

    // Toolbar actions
    void newPattern();
    void setPatternBars (int bars);
    void loopPattern();
    void clearPattern();
    void applySwing (double newPercent);

    EditSession& session;
    tracktion::Track::Ptr track;
    tracktion::MidiClip::Ptr clip;

    juce::TextButton newButton { "New Pattern" }, loopButton { "Loop Pattern" }, clearButton { "Clear" };
    juce::ComboBox lengthBox;
    juce::Label swingLabel { {}, "Swing" }, hintLabel;
    juce::Slider swingSlider;

    bool painting = false, paintOn = true;
    int lastRow = -1, lastStep = -1;

    static constexpr int toolbarHeight = 40;
    static constexpr int labelWidth    = 150;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSequencer)
};
