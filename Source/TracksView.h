#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "TrackHeader.h"
#include "TrackLane.h"
#include "AutomationLane.h"

class TracksView : public juce::Component,
                   private juce::Timer
{
public:
    explicit TracksView (EditSession& sessionToUse);
    ~TracksView() override;

    void resized() override;
    int getIdealHeight() const;

private:
    void timerCallback() override;
    void rebuild();
    void updateLayout();
    juce::Array<tracktion::AudioTrack*> getCurrentTracks();
    juce::String getAutomationSignature() const;

    EditSession& session;

    juce::OwnedArray<TrackHeader> headers;
    juce::OwnedArray<TrackLane> lanes;
    juce::OwnedArray<AutomationHeader> autoHeaders;
    juce::OwnedArray<AutomationLane> autoLanes;

    juce::TextButton addTrackButton { "+ Add Track" };
    juce::TextButton addMidiTrackButton { "+ Add MIDI Track" };
    juce::TextButton addDrumTrackButton { "+ Add Drum Track" };

    // Rebuild when tracks are added, removed or reordered; relayout when lanes show or hide
    juce::Array<tracktion::AudioTrack*> builtTracks;
    juce::String lastAutoSignature;

    static constexpr int rowHeight   = 112;
    static constexpr int autoHeight  = 80;
    static constexpr int headerWidth = 240;
    static constexpr int gap         = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TracksView)
};
