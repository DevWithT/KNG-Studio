#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

class TrackLane : public juce::Component,
                  private juce::Timer
{
public:
    TrackLane (EditSession& sessionToUse, int trackIndexToUse);
    ~TrackLane() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct ClipThumb
    {
        tracktion::Clip::Ptr clip;
        std::unique_ptr<tracktion::SmartThumbnail> thumb;
    };

    void timerCallback() override;
    void syncClips();
    void syncRecordingThumb();
    void drawClips (juce::Graphics&);
    void drawRecording (juce::Graphics&);
    tracktion::Clip* findClipAt (int x);
    int timeToX (double seconds) const;

    EditSession& session;
    const int trackIndex;

    std::vector<ClipThumb> clipThumbs;
    tracktion::RecordingThumbnailManager::Thumbnail::Ptr recordingThumb;

    static constexpr double viewSeconds = 60.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackLane)
};
