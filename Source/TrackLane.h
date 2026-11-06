#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

class TrackLane : public juce::Component,
                  public juce::FileDragAndDropTarget,
                  private juce::Timer
{
public:
    TrackLane (EditSession& sessionToUse, int trackIndexToUse);
    ~TrackLane() override;

    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    struct ClipThumb
    {
        tracktion::Clip::Ptr clip;
        std::unique_ptr<tracktion::SmartThumbnail> thumb;
        int take = -1;
    };

    enum class DragMode { none, move, trimStart, trimEnd, fadeIn, fadeOut, box };

    struct MovedClip
    {
        tracktion::Clip::Ptr clip;
        double origStart = 0.0;
    };

    struct DragState
    {
        DragMode mode = DragMode::none;
        tracktion::Clip::Ptr clip;
        double downTime = 0.0, origStart = 0.0, origEnd = 0.0, origOffset = 0.0;
        double origFadeIn = 0.0, origFadeOut = 0.0;
        std::vector<MovedClip> moving;
        int boxStartX = 0, boxEndX = 0;
        bool changed = false;
    };

    // Painting and syncing (TrackLane.cpp)
    void timerCallback() override;
    void syncClips();
    void syncRecordingThumb();
    void drawGrid (juce::Graphics&);
    void drawClips (juce::Graphics&);
    void drawFades (juce::Graphics&, tracktion::Clip&, juce::Rectangle<int> clipArea);
    void drawRecording (juce::Graphics&);
    void drawBox (juce::Graphics&);
    int timeToX (double seconds) const;
    double xToTime (int x) const;

    // Mouse helpers (TrackLaneMouse.cpp)
    tracktion::Clip* findClipAt (int x);
    DragMode getModeAt (tracktion::Clip&, juce::Point<int>) const;
    void showClipMenu (tracktion::Clip*);
    void applyDrag (const juce::MouseEvent&);
    void selectClipsInBox();

    EditSession& session;
    const int trackIndex;

    std::vector<ClipThumb> clipThumbs;
    tracktion::RecordingThumbnailManager::Thumbnail::Ptr recordingThumb;
    DragState drag;

    static constexpr int edgeZone = 6;
    static constexpr int fadeHandleSize = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackLane)
};
