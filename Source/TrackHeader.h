#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "LevelMeter.h"

/** A lane's header in the Playlist: color strip, name, R M S, a meter, and the input when there's room.
    Volume, pan and sends live in the inspector and the Mixer. */
class TrackHeader : public juce::Component,
                    private juce::Timer
{
public:
    TrackHeader (EditSession& sessionToUse, int trackIndexToUse);
    ~TrackHeader() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    static constexpr int stripWidth = 6;

private:
    void timerCallback() override;
    void refreshInputList();

    EditSession& session;
    const int trackIndex;
    const bool isBus;

    juce::Label nameLabel;
    juce::TextButton armButton { "R" }, muteButton { "M" }, soloButton { "S" };
    juce::ComboBox inputBox;
    LevelMeter meter;

    juce::StringArray lastInputNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackHeader)
};
