#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** The thin row above the ruler. Markers show as a label. Sections show as a named block you can drag
    (its clips go with it), resize from the right edge, and duplicate. Right-click to add a section. */
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
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override       { repaint(); }

    int getLabelWidth (tracktion::Clip& marker) const;
    tracktion::Clip* findMarkerAt (int x, bool* onRightEdge = nullptr);
    void renameMarker (tracktion::Clip::Ptr marker);
    void showMarkerMenu (tracktion::Clip* marker);
    void showAddSectionMenu (int x);
    void askSectionName (double start, double length);

    EditSession& session;

    tracktion::Clip::Ptr dragMarker;
    bool dragged = false;
    bool resizing = false;
    double dragOrigStart = 0.0, dragOrigLength = 0.0, dragGrabOffset = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarkerBar)
};
