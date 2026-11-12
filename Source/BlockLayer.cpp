#include "BlockLayer.h"

namespace te = tracktion;

BlockLayer::BlockLayer (EditSession& sessionToUse, int trackIndexToUse)
    : session (sessionToUse), trackIndex (trackIndexToUse)
{
    startTimerHz (10);
}

BlockLayer::~BlockLayer()
{
    stopTimer();
}

juce::String BlockLayer::laneId()
{
    return session.getLaneId (trackIndex);
}

bool BlockLayer::laneMuted()
{
    auto* t = session.getTrack (trackIndex);
    return t != nullptr && t->isMuted (false);
}

std::vector<EditSession::PatternBlock> BlockLayer::myBlocks()
{
    std::vector<EditSession::PatternBlock> mine;
    const auto id = laneId();

    for (auto& b : session.getPatternBlocks())
        if (b.lane == id)
            mine.push_back (b);

    return mine;
}

bool BlockLayer::blockAt (double seconds, EditSession::PatternBlock& out)
{
    for (auto& b : myBlocks())
    {
        if (seconds >= b.start && seconds < b.end)
        {
            out = b;
            return true;
        }
    }

    return false;
}

bool BlockLayer::onRightEdge (const EditSession::PatternBlock& b, int x)
{
    return x >= session.timeToX (b.end, getWidth()) - edgeZone;
}

bool BlockLayer::hitTest (int x, int)
{
    // Paint takes every click; Select and Cut only take clicks on blocks (the rest goes to the audio below)
    if (session.getPlaylistTool() == 1)
        return true;

    EditSession::PatternBlock b;
    return blockAt (session.xToTime (x, getWidth()), b);
}

//==============================================================================
void BlockLayer::paint (juce::Graphics& g)
{
    const int current = session.getCurrentPattern();
    const bool muted = laneMuted();

    for (auto& b : myBlocks())
    {
        const int x1 = session.timeToX (b.start, getWidth());
        const int x2 = session.timeToX (b.end, getWidth());

        if (x2 < 0 || x1 > getWidth())
            continue;

        auto r = juce::Rectangle<int> (x1, 3, juce::jmax (4, x2 - x1), getHeight() - 6).toFloat().reduced (1.0f, 0.0f);
        const bool dragged = (moving || resizing) && b.pattern == dragBlock.pattern && std::abs (b.start - dragBlock.start) < 0.001;
        const auto colour = session.getPatternColour (b.pattern);

        g.setColour (colour.withAlpha (muted ? 0.3f : dragged ? 0.35f : 0.92f));
        g.fillRoundedRectangle (r, 4.0f);

        // Name strip
        auto strip = r.withHeight (16.0f);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRoundedRectangle (strip, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (muted ? 0.5f : 1.0f));
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (session.getPatternName (b.pattern), strip.toNearestInt().reduced (6, 0), juce::Justification::centredLeft, true);

        // A hint of notes
        g.setColour (juce::Colours::white.withAlpha (muted ? 0.15f : 0.35f));

        for (float x = r.getX() + 6.0f; x < r.getRight() - 8.0f; x += 10.0f)
            g.fillRect (x, r.getCentreY() + 4.0f, 5.0f, 3.0f);

        g.setColour (b.pattern == current ? juce::Colours::white : juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 4.0f, b.pattern == current ? 1.5f : 1.0f);
    }

    // Where a dragged block will land
    if (moving && std::abs (dropTime - dragBlock.start) > 0.001)
    {
        const int x1 = session.timeToX (dropTime, getWidth());
        const int x2 = session.timeToX (dropTime + (dragBlock.end - dragBlock.start), getWidth());
        auto r = juce::Rectangle<int> (x1, 3, juce::jmax (4, x2 - x1), getHeight() - 6).toFloat();

        g.setColour (session.getPatternColour (dragBlock.pattern).withAlpha (0.55f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (r, 4.0f, 1.5f);
    }

    // The new length while stretching
    if (resizing)
    {
        const int x1 = session.timeToX (dragBlock.start, getWidth());
        const int x2 = session.timeToX (resizeEnd, getWidth());
        auto r = juce::Rectangle<int> (x1, 3, juce::jmax (4, x2 - x1), getHeight() - 6).toFloat();

        g.setColour (session.getPatternColour (dragBlock.pattern).withAlpha (0.55f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (r, 4.0f, 1.5f);
    }
}

//==============================================================================
void BlockLayer::paintAt (int x)
{
    const double start = session.barStartAt (juce::jmax (0.0, session.xToTime (x, getWidth())));

    if (std::abs (start - lastPaintBar) < 0.001)
        return;

    lastPaintBar = start;

    const int pattern = session.getCurrentPattern();
    const double end = session.barsAfter (start, session.getPatternBars (pattern));

    for (auto& b : myBlocks())
        if (b.start < end - 0.001 && b.end > start + 0.001)
            return;

    session.placePatternInLane (pattern, start, laneId());
}

void BlockLayer::eraseAt (int x)
{
    EditSession::PatternBlock b;

    if (blockAt (session.xToTime (x, getWidth()), b))
        session.deleteBlock (b);
}

void BlockLayer::showMenu (EditSession::PatternBlock block)
{
    auto* s = &session;
    juce::PopupMenu menu;

    menu.addItem ("Edit in Rack", [s, block]
    {
        s->setCurrentPattern (block.pattern);

        if (s->onShowRack)
            s->onShowRack();
    });

    menu.addItem ("Make Unique", [s, block]
    {
        for (auto* c : s->getBlockClips (block))
        {
            if (auto* mc = dynamic_cast<te::MidiClip*> (c))
            {
                s->makePatternUnique (*mc);
                break;
            }
        }
    });

    menu.addSeparator();
    menu.addItem ("Delete", [s, block] { juce::MessageManager::callAsync ([s, block] { s->deleteBlock (block); }); });
    menu.showMenuAsync ({});
}

//==============================================================================
void BlockLayer::mouseMove (const juce::MouseEvent& e)
{
    const int tool = session.getPlaylistTool();
    EditSession::PatternBlock b;

    if (tool == 2)
        setMouseCursor (juce::MouseCursor::IBeamCursor);
    else if (tool == 0 && blockAt (session.xToTime (e.x, getWidth()), b) && onRightEdge (b, e.x))
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (tool == 1)
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    else
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void BlockLayer::mouseDown (const juce::MouseEvent& e)
{
    moving = painting = erasing = resizing = false;
    lastPaintBar = -1.0;

    if (session.getTransport().isRecording())
        return;

    const int tool = session.getPlaylistTool();
    const double t = session.xToTime (e.x, getWidth());
    EditSession::PatternBlock hit;
    const bool onBlock = blockAt (t, hit);

    if (e.mods.isPopupMenu())
    {
        // Paint: right-drag erases. Select and Cut: right-click a block for its menu.
        if (tool == 1)
        {
            erasing = true;
            eraseAt (e.x);
        }
        else if (onBlock)
        {
            showMenu (hit);
        }

        return;
    }

    // Cut: slice the block where you click
    if (tool == 2)
    {
        if (onBlock)
            session.cutBlock (hit, t);

        return;
    }

    if (onBlock)
    {
        session.setCurrentPattern (hit.pattern);
        dragBlock = hit;

        // Select: the right edge stretches, anywhere else moves
        if (tool == 0 && onRightEdge (hit, e.x))
        {
            resizing = true;
            resizeEnd = hit.end;
            return;
        }

        moving = true;
        grab = t - hit.start;
        dropTime = hit.start;
        return;
    }

    if (tool == 1)
    {
        painting = true;
        paintAt (e.x);
    }
}

void BlockLayer::mouseDrag (const juce::MouseEvent& e)
{
    if (erasing)
    {
        eraseAt (e.x);
    }
    else if (painting)
    {
        paintAt (e.x);
    }
    else if (resizing)
    {
        const double t = session.xToTime (e.x, getWidth());
        resizeEnd = juce::jmax (dragBlock.start + 0.01, session.isSnapEnabled() && ! e.mods.isAltDown() ? session.snapTime (t) : t);
        repaint();
    }
    else if (moving)
    {
        // Land on the nearest bar line
        const double t = juce::jmax (0.0, session.xToTime (e.x, getWidth()) - grab);
        const double before = session.barStartAt (t);
        const double after = session.barsAfter (before, 1);
        dropTime = (t - before) < (after - t) ? before : after;
        repaint();
    }
}

void BlockLayer::mouseUp (const juce::MouseEvent& e)
{
    if (resizing && std::abs (resizeEnd - dragBlock.end) > 0.001)
    {
        session.resizeBlock (dragBlock, resizeEnd);
    }
    else if (moving && e.getDistanceFromDragStart() > 3)
    {
        auto lane = laneAtScreen ? laneAtScreen (e.getScreenPosition()) : laneId();

        if (lane.isEmpty())
            lane = dragBlock.lane;

        session.moveBlock (dragBlock, dropTime, lane, e.mods.isCtrlDown());
    }

    moving = painting = erasing = resizing = false;
    lastPaintBar = -1.0;
    repaint();
}

void BlockLayer::mouseDoubleClick (const juce::MouseEvent& e)
{
    EditSession::PatternBlock b;

    if (blockAt (session.xToTime (e.x, getWidth()), b))
    {
        session.setCurrentPattern (b.pattern);

        if (session.onShowRack)
            session.onShowRack();
    }
}
