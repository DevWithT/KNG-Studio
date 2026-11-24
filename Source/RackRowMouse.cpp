#include "RackRow.h"
#include "PatternGrid.h"
#include "VolumeHelpers.h"
#include "KngSamplerPlugin.h"
#include "KngSynthPlugin.h"
#include <map>

namespace te = tracktion;

//==============================================================================
void RackRow::mouseDown (const juce::MouseEvent& e)
{
    const int idx = getTrackIndex();

    if (idx < 0)
        return;

    const auto p = e.getPosition();
    auto lane = getLaneArea();
    const bool rightButton = e.mods.isRightButtonDown() || e.mods.isPopupMenu();

    // Clicking a row shows its track in the inspector on the right
    session.selectTrack (idx);

    // The step graph under the row: draw velocity or pitch
    if (getGraphMode() > 0 && getGraphArea().contains (p))
    {
        session.beginEdit();
        graphEditing = true;
        editGraphAt (p);
        return;
    }

    // Left side: right-click for the menu, left-click to hear the sound
    if (p.x < lane.getX())
    {
        if (rightButton && p.y < rowHeight)
        {
            showRowMenu();
            return;
        }

        if (isDrum() && isExpanded() && p.y >= rowHeight)
        {
            auto pads = getVisiblePads();
            const int sub = (p.y - rowHeight) / padRowHeight;

            if (juce::isPositiveAndBelow (sub, pads.size()))
                session.playPad (idx, pads[sub], 110);
        }
        else if (isChannel() && ! rightButton)
        {
            preview (getChannelNote());
        }

        return;
    }

    // Melodic rows open the piano roll for this pattern
    if (! isDrum() && ! isChannel())
    {
        if (auto* clip = getClip (true))
            if (session.onShowPianoRoll)
                session.onShowPianoRoll (*clip);

        return;
    }

    // Clicking the summary strip opens a closed drum row
    if (isDrum() && p.y < rowHeight)
    {
        if (! isExpanded())
            setExpanded (true);

        return;
    }

    int note = 0, step = 0;

    if (! hitAt (p, note, step))
        return;

    auto* clip = getClip (true);

    if (clip == nullptr)
        return;

    // Sound rows find a step's hit whatever pitch the graph moved it to
    const bool channel = isChannel();
    paintOn = ! rightButton && (channel ? noteAtStep (*clip, step, -1) : PatternGrid::findNote (*clip, note, step)) == nullptr;

    // The kit's step graph follows the pad you last clicked
    if (isDrum())
        track->state.setProperty ("kngGraphPad", note - EditSession::getPadNote (0), nullptr);

    session.beginEdit();
    painting = true;

    // Placing a step in a locked bar unlocks it
    if (paintOn)
        growToStep (step);
    lastNote = note;
    lastStep = step;

    if (channel)
        setChannelStep (*clip, step, paintOn);
    else
        PatternGrid::setStep (*clip, note, step, paintOn, &session.getEdit().getUndoManager());

    if (paintOn)
        preview (note);

    repaint();
}

// A step placed in a locked bar unlocks it: the pattern grows to take in that whole bar
void RackRow::growToStep (int step)
{
    const int pattern = session.getCurrentPattern();
    const int live = session.getPatternSteps (pattern);

    if (step < live)
        return;

    const int perBar = juce::jmax (1, live / juce::jmax (1, session.getPatternBars (pattern)));
    session.setPatternBars (pattern, step / perBar + 1);

    // The Rack is wider now
    if (onLayoutChanged)
        onLayoutChanged();
}

// Shorten the current pattern to its last bar with a step or note on any sound (never under 2 bars)
void RackRow::trimToContent (EditSession& session)
{
    const int pattern = session.getCurrentPattern();
    const int bars = session.getPatternBars (pattern);
    const int perBar = juce::jmax (1, session.getPatternSteps (pattern) / juce::jmax (1, bars));
    int lastStep = 0;

    for (int i = 0; i < session.getNumTracks(); ++i)
    {
        if (auto* clip = session.getPatternClip (i, pattern, false))
        {
            const double start = PatternGrid::clipStartBeat (*clip);

            for (auto* n : clip->getSequence().getNotes())
                lastStep = juce::jmax (lastStep, PatternGrid::beatToStep (n->getStartBeat().inBeats() - start));
        }
    }

    const int wanted = juce::jmax (2, lastStep / perBar + 1);

    if (wanted < bars)
        session.setPatternBars (pattern, wanted);
}

void RackRow::mouseDrag (const juce::MouseEvent& e)
{
    if (graphEditing)
    {
        editGraphAt (e.getPosition());
        return;
    }

    // Dragging a row by its name drops the current pattern into the Playlist
    if (! painting)
    {
        if (! e.mods.isPopupMenu() && e.getDistanceFromDragStart() > 8
            && e.getMouseDownX() < getLaneArea().getX() && e.getMouseDownY() < rowHeight)
        {
            if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor (this))
                if (! dnd->isDragAndDropActive())
                    dnd->startDragging ("kngpattern|" + juce::String (session.getCurrentPattern()), this);
        }

        return;
    }

    int note = 0, step = 0;

    // Dragging into a locked bar unlocks it
    if (paintOn && hitAt (e.getPosition(), note, step) && step >= getLiveSteps())
        growToStep (step);

    if (hitAt (e.getPosition(), note, step) && (note != lastNote || step != lastStep))
    {
        lastNote = note;
        lastStep = step;

        if (auto* clip = getClip (false))
        {
            if (isChannel())
                setChannelStep (*clip, step, paintOn);
            else
                PatternGrid::setStep (*clip, note, step, paintOn, &session.getEdit().getUndoManager());
        }

        repaint();
    }
}

void RackRow::mouseUp (const juce::MouseEvent&)
{
    if (graphEditing)
    {
        graphEditing = false;

        if (auto* clip = getClip (false))
            if (clip->isLinked())
                clip->updateLinkedClips();

        session.save();
        return;
    }

    if (! painting)
        return;

    painting = false;

    // Erasing steps can empty the last bars: the pattern shrinks back and they lock again
    if (! paintOn)
    {
        trimToContent (session);

        if (onLayoutChanged)
            onLayoutChanged();
    }

    if (auto* clip = getClip (false))
    {
        // Sound rows: each hit rings until the next one (whatever its pitch)
        if (isChannel())
            makeChannelLegato (*clip);

        // Push the change to every linked copy on the timeline
        if (clip->isLinked())
            clip->updateLinkedClips();
    }

    session.save();
}
