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

    paintOn = ! rightButton && PatternGrid::findNote (*clip, note, step) == nullptr;

    session.beginEdit();
    painting = true;
    lastNote = note;
    lastStep = step;

    PatternGrid::setStep (*clip, note, step, paintOn, &session.getEdit().getUndoManager());

    if (paintOn)
        preview (note);

    repaint();
}

void RackRow::mouseDrag (const juce::MouseEvent& e)
{
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

    if (hitAt (e.getPosition(), note, step) && (note != lastNote || step != lastStep))
    {
        lastNote = note;
        lastStep = step;

        if (auto* clip = getClip (false))
            PatternGrid::setStep (*clip, note, step, paintOn, &session.getEdit().getUndoManager());

        repaint();
    }
}

void RackRow::mouseUp (const juce::MouseEvent&)
{
    if (! painting)
        return;

    painting = false;

    if (auto* clip = getClip (false))
    {
        // Sound rows: each hit rings until the next one
        if (isChannel())
            PatternGrid::fillToNext (*clip, getChannelNote(), getNumSteps(), &session.getEdit().getUndoManager());

        // Push the change to every linked copy on the timeline
        if (clip->isLinked())
            clip->updateLinkedClips();
    }

    session.save();
}
