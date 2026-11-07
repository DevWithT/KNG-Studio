#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** One instrument in the Rack: controls on the left, its pattern on the right. */
class RackRow : public juce::Component,
                private juce::Timer
{
public:
    RackRow (EditSession& sessionToUse, tracktion::Track& trackToUse);
    ~RackRow() override;

    int getIdealHeight();
    tracktion::Track* getTrack() const   { return track.get(); }

    /** Called when the row grows or shrinks (a drum row opens or closes). */
    std::function<void()> onLayoutChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    int getTrackIndex();
    tracktion::AudioTrack* getAudioTrack();
    bool isDrum();
    bool isExpanded();
    void setExpanded (bool shouldBeOpen);
    juce::Array<int> getVisiblePads();
    int getNumSteps();
    tracktion::MidiClip* getClip (bool createIfMissing);

    juce::Rectangle<int> getLaneArea() const;
    juce::Rectangle<int> getStepBounds (int subRow, int step);
    bool stepAt (juce::Point<int>, int& pad, int& step);
    int getPlayStep (tracktion::MidiClip* clip);

    void syncControls();
    void openInstrument();
    void showInstrumentMenu();

    void paintDrumSummary (juce::Graphics&, tracktion::MidiClip*);
    void paintDrumRows (juce::Graphics&, tracktion::MidiClip*);
    void paintMelodic (juce::Graphics&, tracktion::MidiClip*);

    EditSession& session;
    tracktion::Track::Ptr track;

    juce::TextButton expandButton, muteButton { "M" }, soloButton { "S" }, instrumentButton;
    juce::Label nameLabel;
    juce::Slider volKnob, panKnob;

    bool painting = false, paintOn = true;
    int lastPad = -1, lastStep = -1;

    static constexpr int headerWidth  = 300;
    static constexpr int rowHeight    = 36;
    static constexpr int padRowHeight = 24;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackRow)
};
