#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** Strip above the tracks: paint, move, copy and erase pattern copies across bars. */
class PatternLane : public juce::Component,
                    private juce::Timer
{
public:
    explicit PatternLane (EditSession& sessionToUse);
    ~PatternLane() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override   { repaint(); }
    double barStartAt (int x);
    void paintAt (int x);
    void eraseAt (int x);
    static juce::Colour colourFor (int pattern);

    EditSession& session;
    bool painting = false, erasing = false, moving = false;
    double lastBar = -1.0;

    // Dragging a block
    int movePattern = 0;
    double moveFrom = 0.0, moveLength = 0.0, moveGrab = 0.0, moveTo = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternLane)
};
