#include "BlockLayer.h"
#include "KngTheme.h"
#include "PatternWav.h"

namespace te = tracktion;

// The grid line at or before this time. Snap off or Alt held: the time itself.
static double gridFloor (EditSession& session, double seconds)
{
    seconds = juce::jmax (0.0, seconds);

    if (! session.isSnapEnabled() || juce::ModifierKeys::currentModifiers.isAltDown())
        return seconds;

    const int grid = session.getSnapGrid();

    if (grid <= 0)
        return session.barStartAt (seconds);

    auto& ts = session.getEdit().tempoSequence;
    const double beat = std::floor (ts.toBeats (te::TimePosition::fromSeconds (seconds)).inBeats() * grid + 1.0e-6) / grid;
    return ts.toTime (te::BeatPosition::fromBeats (beat)).inSeconds();
}

// A block's real notes, from every sound in it (stretched blocks show each repeat). False if it has none.
static bool drawBlockNotes (juce::Graphics& g, EditSession& session, const EditSession::PatternBlock& b,
                            juce::Rectangle<float> area, int width, juce::Colour noteColour, float alpha)
{
    auto& ts = session.getEdit().tempoSequence;
    const auto clips = session.getBlockClips (b);
    int lo = 127, hi = 0;

    for (auto* c : clips)
        if (auto* mc = dynamic_cast<te::MidiClip*> (c))
            for (auto* n : mc->getSequence().getNotes())
            {
                lo = juce::jmin (lo, n->getNoteNumber());
                hi = juce::jmax (hi, n->getNoteNumber());
            }

    if (hi < lo)
        return false;

    const float span = (float) juce::jmax (12, hi - lo + 1);
    g.setColour (noteColour.withAlpha (alpha));

    for (auto* c : clips)
    {
        auto* mc = dynamic_cast<te::MidiClip*> (c);

        if (mc == nullptr)
            continue;

        const auto pos = mc->getPosition();
        const double startBeat = ts.toBeats (pos.getStart()).inBeats();
        const double endBeat = ts.toBeats (pos.getEnd()).inBeats();
        const bool looped = mc->isLooping();
        const double loopStart = looped ? mc->getLoopStartBeats().inBeats()
                                        : mc->getContentBeatAtTime (pos.getStart()).inBeats();
        const double loopLen = looped ? juce::jmax (0.25, mc->getLoopLengthBeats().inBeats()) : 1.0e9;

        for (auto* n : mc->getSequence().getNotes())
        {
            const double ns = n->getStartBeat().inBeats();
            const double ne = n->getEndBeat().inBeats();

            if (looped && (ns < loopStart || ns >= loopStart + loopLen))
                continue;

            for (int k = 0; k < 64; ++k)
            {
                const double a = startBeat + (ns - loopStart) + k * loopLen;

                if (a >= endBeat)
                    break;

                const double z = juce::jmin (endBeat, startBeat + (ne - loopStart) + k * loopLen);

                if (z > startBeat)
                {
                    const float x1 = (float) session.timeToX (ts.toTime (te::BeatPosition::fromBeats (juce::jmax (startBeat, a))).inSeconds(), width);
                    const float x2 = (float) session.timeToX (ts.toTime (te::BeatPosition::fromBeats (z)).inSeconds(), width);
                    const float y = area.getY() + (1.0f - (n->getNoteNumber() - lo + 0.5f) / span) * area.getHeight();
                    g.fillRect (x1, y - 1.0f, juce::jmax (2.0f, x2 - x1 - 1.0f), 2.5f);
                }

                if (! looped)
                    break;
            }
        }
    }

    return true;
}

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
        const float fade = muted ? 0.5f : dragged ? 0.4f : 1.0f;

        // Tinted body
        g.setColour (colour.withAlpha (0.22f * fade));
        g.fillRoundedRectangle (r, 2.0f);

        // Solid name band along the top
        auto band = r.withHeight (14.0f);
        juce::Path top;
        top.addRoundedRectangle (band.getX(), band.getY(), band.getWidth(), band.getHeight(), 2.0f, 2.0f, true, true, false, false);
        g.setColour (colour.withAlpha (fade));
        g.fillPath (top);

        g.setColour (KngTheme::ink.withAlpha (fade));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText (session.getPatternName (b.pattern), band.toNearestInt().reduced (5, 0), juce::Justification::centredLeft, true);

        // The pattern's real notes
        {
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (r.toNearestInt());

            if (! drawBlockNotes (g, session, b, r.withTrimmedTop (16.0f).reduced (2.0f, 3.0f), getWidth(), colour, fade))
            {
                g.setColour (KngTheme::muted.withAlpha (fade));
                g.setFont (juce::Font (juce::FontOptions (11.0f)));
                g.drawText ("Empty", r.withTrimmedTop (14.0f).toNearestInt(), juce::Justification::centred);
            }
        }

        // Outline: accent for the pattern you're editing, lane color for the rest
        const bool isCurrent = b.pattern == current;
        g.setColour (isCurrent ? KngTheme::accent : colour.withAlpha (0.9f * fade));
        g.drawRoundedRectangle (r, 2.0f, isCurrent ? 2.0f : 1.0f);
    }

    // Where a dragged block will land
    if (moving && std::abs (dropTime - dragBlock.start) > 0.001)
    {
        const int x1 = session.timeToX (dropTime, getWidth());
        const int x2 = session.timeToX (dropTime + (dragBlock.end - dragBlock.start), getWidth());
        auto r = juce::Rectangle<int> (x1, 3, juce::jmax (4, x2 - x1), getHeight() - 6).toFloat();

        g.setColour (session.getPatternColour (dragBlock.pattern).withAlpha (0.45f));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (KngTheme::accent);
        g.drawRoundedRectangle (r, 2.0f, 1.5f);
    }

    // The new length while stretching
    if (resizing)
    {
        const int x1 = session.timeToX (dragBlock.start, getWidth());
        const int x2 = session.timeToX (resizeEnd, getWidth());
        auto r = juce::Rectangle<int> (x1, 3, juce::jmax (4, x2 - x1), getHeight() - 6).toFloat();

        g.setColour (session.getPatternColour (dragBlock.pattern).withAlpha (0.45f));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (KngTheme::accent);
        g.drawRoundedRectangle (r, 2.0f, 1.5f);
    }
}

//==============================================================================
void BlockLayer::paintAt (int x)
{
    const double start = gridFloor (session, session.xToTime (x, getWidth()));

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

    menu.addItem ("Convert to WAV", [s, block]
    {
        KngPatternWav::render (*s, block, [] (const juce::File& file, const juce::String& error)
        {
            if (error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Convert to WAV",
                                                        "Couldn't render this pattern: " + error);
            else
                file.revealToUser();
        });
    });

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
        session.setDeferSave (true);
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
        // Land on the nearest grid line (Snap off or Alt held: anywhere)
        const double t = juce::jmax (0.0, session.xToTime (e.x, getWidth()) - grab);
        dropTime = e.mods.isAltDown() ? t : session.snapTime (t);
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

    // A paint stroke saves once, at the end
    if (painting)
    {
        session.setDeferSave (false);
        session.save();
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
