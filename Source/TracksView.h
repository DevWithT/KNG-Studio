#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "TrackHeader.h"
#include "TrackLane.h"
#include "AutomationLane.h"
#include "FolderHeader.h"
#include "BlockLayer.h"

/** The Playlist: one row per lane (audio track). Instruments live in the Rack, buses in the Mixer. */
class TracksView : public juce::Component,
                   public juce::DragAndDropTarget,
                   private juce::Timer
{
public:
    explicit TracksView (EditSession& sessionToUse);
    ~TracksView() override;

    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setLaneHeight (int newHeight);
    int getIdealHeight();

    // Plugins from the Plugins browser and patterns from the Patterns list
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails& d) override   { hoverY = d.localPosition.y; repaint(); }
    void itemDragExit (const SourceDetails&) override     { hoverY = -1; repaint(); }
    void itemDropped (const SourceDetails&) override;
    void paintOverChildren (juce::Graphics&) override;

private:
    void timerCallback() override;
    void rebuild();
    void updateLayout();
    int layout (bool apply);
    int trackAtY (int y) const;
    juce::Array<tracktion::AudioTrack*> getCurrentTracks();
    juce::String getLayoutSignature();

    EditSession& session;

    juce::OwnedArray<TrackHeader> headers;
    juce::OwnedArray<TrackLane> lanes;
    juce::OwnedArray<AutomationHeader> autoHeaders;
    juce::OwnedArray<AutomationLane> autoLanes;
    juce::OwnedArray<FolderHeader> folderHeaders;
    juce::OwnedArray<BlockLayer> blockLayers;   // one per track index; null for tracks that aren't lanes

    juce::TextButton addTrackButton { "+ Add lane" };

    juce::Array<tracktion::AudioTrack*> builtTracks;
    juce::String lastSignature;
    int hoverY = -1;

    int rowHeight = 64;   // Ctrl+scroll over the lane headers changes it; saved with the song
    static constexpr int minRowHeight = 40;
    static constexpr int maxRowHeight = 160;
    static constexpr int autoHeight   = 80;
    static constexpr int headerWidth  = 240;
    static constexpr int gap          = 4;
    static constexpr int folderHeight = 30;
    static constexpr int indent       = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TracksView)
};
