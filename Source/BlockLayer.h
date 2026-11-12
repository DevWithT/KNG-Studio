#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** Sits over one lane and draws its pattern blocks. Select: drag blocks. Paint: click to place the current pattern. */
class BlockLayer : public juce::Component,
                   private juce::Timer
{
public:
    BlockLayer (EditSession& sessionToUse, int trackIndexToUse);
    ~BlockLayer() override;

    /** Set by the tracks view: the lane under a point on screen (empty if none). */
    std::function<juce::String (juce::Point<int>)> laneAtScreen;

    bool hitTest (int x, int y) override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override   { repaint(); }
    juce::String laneId();
    std::vector<EditSession::PatternBlock> myBlocks();
    bool blockAt (double seconds, EditSession::PatternBlock& out);
    void paintAt (int x);
    void eraseAt (int x);
    void showMenu (EditSession::PatternBlock block);

    EditSession& session;
    const int trackIndex;

    bool moving = false, painting = false, erasing = false;
    EditSession::PatternBlock dragBlock;
    double grab = 0.0, dropTime = 0.0, lastPaintBar = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockLayer)
};
