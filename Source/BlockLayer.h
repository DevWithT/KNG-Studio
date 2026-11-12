#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** Sits over one lane and draws its pattern blocks.
    Select: drag blocks, drag a right edge to stretch. Paint: click to place. Cut: click to slice. */
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
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override   { repaint(); }
    juce::String laneId();
    bool laneMuted();
    std::vector<EditSession::PatternBlock> myBlocks();
    bool blockAt (double seconds, EditSession::PatternBlock& out);
    bool onRightEdge (const EditSession::PatternBlock& b, int x);
    void paintAt (int x);
    void eraseAt (int x);
    void showMenu (EditSession::PatternBlock block);

    EditSession& session;
    const int trackIndex;

    bool moving = false, painting = false, erasing = false, resizing = false;
    EditSession::PatternBlock dragBlock;
    double grab = 0.0, dropTime = 0.0, resizeEnd = 0.0, lastPaintBar = -1.0;

    static constexpr int edgeZone = 6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockLayer)
};
