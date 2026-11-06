#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** Thin row above the ruler for song section markers. */
class MarkerBar : public juce::Component,
                  private juce::Timer
{
public:
    explicit MarkerBar (EditSession& sessionToUse);
    ~MarkerBar() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override   { repaint(); }
    tracktion::Clip* findMarkerAt (int x);
    int getLabelWidth (tracktion::Clip&) const;
    void renameMarker (tracktion::Clip::Ptr marker);

    EditSession& session;
    tracktion::Clip::Ptr dragMarker;
    bool dragged = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarkerBar)
};
