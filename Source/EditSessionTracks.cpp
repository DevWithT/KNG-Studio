#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
void EditSession::moveTrack (int index, int direction)
{
    if (getTransport().isRecording())
        return;

    syncTrackState();

    const int target = index + direction;
    auto* track = getTrack (index);
    auto* other = getTrack (target);

    if (track == nullptr || other == nullptr)
        return;

    beginEdit();
    edit->moveTrack (te::Track::Ptr (track), te::TrackInsertPoint (*other, direction < 0));

    // Keep each track's input and arm state with it
    const int tmpInput = trackInputs[(size_t) index];
    trackInputs[(size_t) index] = trackInputs[(size_t) target];
    trackInputs[(size_t) target] = tmpInput;

    const bool tmpArmed = trackArmed[(size_t) index];
    trackArmed[(size_t) index] = trackArmed[(size_t) target];
    trackArmed[(size_t) target] = tmpArmed;

    save();
}

//==============================================================================
void EditSession::duplicateSelectedClip()
{
    if (selectedClip == nullptr || getTransport().isRecording())
        return;

    beginEdit();

    const auto pos = selectedClip->getPosition();
    auto newClip = te::duplicateClip (*selectedClip);

    if (newClip != nullptr)
    {
        // Place the copy right after the original
        newClip->setStart (pos.getEnd(), false, true);
        selectedClip = newClip;
        save();
    }
}

void EditSession::copySelectedClip()
{
    copiedClip = selectedClip;
}

void EditSession::pasteClip()
{
    if (copiedClip == nullptr || copiedClip->getClipTrack() == nullptr || getTransport().isRecording())
        return;

    // Paste onto the lane you last clicked, or the original's track
    te::ClipTrack* target = getTrack (focusedTrack);

    if (target == nullptr)
        target = copiedClip->getClipTrack();

    beginEdit();

    auto newClip = te::duplicateClip (*copiedClip);

    if (newClip == nullptr)
        return;

    if (target != nullptr && target != newClip->getClipTrack())
        newClip->moveTo (*target);

    newClip->setStart (getTransport().getPosition(), false, true);
    selectedClip = newClip;
    save();
}
