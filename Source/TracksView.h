#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "TrackHeader.h"
#include "TrackLane.h"

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
    juce::Array<tracktion::AudioTrack*> getCurrentTracks();

    EditSession& session;

    juce::OwnedArray<TrackHeader> headers;
    juce::OwnedArray<TrackLane> lanes;
    juce::TextButton addTrackButton { "+ Add Track" };
    juce::TextButton addMidiTrackButton { "+ Add MIDI Track" };

    // Rebuild when tracks are added, removed or reordered
    juce::Array<tracktion::AudioTrack*> builtTracks;

    static constexpr int rowHeight   = 112;
    static constexpr int headerWidth = 240;
    static constexpr int gap         = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TracksView)
};
