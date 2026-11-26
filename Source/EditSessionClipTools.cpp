#include "EditSession.h"

#include <algorithm>
#include <vector>

namespace te = tracktion;

//==============================================================================
// Clip and lane tools: cut, invert selection, swap two clips, duplicate a lane.

void EditSession::cutSelectedClip()
{
    if (getTransport().isRecording() || getSelectedClips().empty())
        return;

    copySelectedClip();
    deleteSelectedClip();
}

void EditSession::invertSelection()
{
    if (getTransport().isRecording())
        return;

    std::vector<te::EditItemID> was;

    for (auto& c : getSelectedClips())
        was.push_back (c->itemID);

    selectAllClips();
    const auto all = getSelectedClips();
    juce::Array<te::Clip*> rest;

    for (auto& c : all)
        if (std::find (was.begin(), was.end(), c->itemID) == was.end())
            rest.add (c.get());

    clearSelection();
    addClipsToSelection (rest);
}

bool EditSession::swapSelectedClips()
{
    if (getTransport().isRecording())
        return false;

    const auto clips = getSelectedClips();

    if (clips.size() != 2)
        return false;

    te::Clip* first = clips[0].get();
    te::Clip* second = clips[1].get();

    if (second->getPosition().getStart().inSeconds() < first->getPosition().getStart().inSeconds())
        std::swap (first, second);

    const double startFirst  = first->getPosition().getStart().inSeconds();
    const double lenFirst    = first->getPosition().getLength().inSeconds();
    const double startSecond = second->getPosition().getStart().inSeconds();
    const double lenSecond   = second->getPosition().getLength().inSeconds();

    beginEdit ("Swap clips");

    if (first->getTrack() == second->getTrack())
    {
        // Same lane: the second one goes first, and the gap between them stays
        const double gap = juce::jmax (0.0, startSecond - (startFirst + lenFirst));
        second->setStart (te::TimePosition::fromSeconds (startFirst), false, true);
        first->setStart (te::TimePosition::fromSeconds (startFirst + lenSecond + gap), false, true);
    }
    else
    {
        // Different lanes: they trade start times
        second->setStart (te::TimePosition::fromSeconds (startFirst), false, true);
        first->setStart (te::TimePosition::fromSeconds (startSecond), false, true);
    }

    save();
    return true;
}

int EditSession::duplicateLane (int trackIndex)
{
    if (getTransport().isRecording() || ! isLaneTrack (trackIndex))
        return -1;

    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return -1;

    const int inputChoice = getTrackInput (trackIndex);

    clearSelection();
    beginEdit ("Duplicate lane");

    te::Track::Ptr copy = edit->copyTrack (te::Track::Ptr (track), te::TrackInsertPoint (*track, false));

    if (copy == nullptr)
        return -1;

    int newIndex = -1;

    for (int i = 0; i < getNumTracks(); ++i)
        if (getTrack (i) == copy.get())
            newIndex = i;

    if (newIndex < 0)
        return -1;

    // Keep the per-lane input and arm lists lined up with the new lane
    syncTrackState();

    trackInputs.insert (trackInputs.begin() + juce::jmin (newIndex, (int) trackInputs.size()), inputChoice);
    trackArmed.insert (trackArmed.begin() + juce::jmin (newIndex, (int) trackArmed.size()), false);
    trackInputs.resize ((size_t) getNumTracks(), 0);
    trackArmed.resize ((size_t) getNumTracks(), false);

    copy->setName (track->getName() + " copy");
    copy->state.setProperty ("kngMonitor", false, nullptr);

    // Grouped clips in the copy get their own group ids
    std::vector<te::Clip::Ptr> copies;

    if (auto* ct = dynamic_cast<te::ClipTrack*> (copy.get()))
        for (auto* c : ct->getClips())
            copies.push_back (c);

    regroupCopies (copies);

    save();
    return newIndex;
}
