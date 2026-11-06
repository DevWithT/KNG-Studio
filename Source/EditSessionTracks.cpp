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
    auto clips = getSelectedClips();

    if (clips.empty() || getTransport().isRecording())
        return;

    // Duplicate the whole group right after itself
    double groupStart = 1.0e12, groupEnd = 0.0;

    for (auto& c : clips)
    {
        groupStart = juce::jmin (groupStart, c->getPosition().getStart().inSeconds());
        groupEnd   = juce::jmax (groupEnd,   c->getPosition().getEnd().inSeconds());
    }

    const double offset = groupEnd - groupStart;

    beginEdit();
    std::vector<te::Clip::Ptr> copies;

    for (auto& c : clips)
    {
        const double start = c->getPosition().getStart().inSeconds();

        if (auto copy = te::duplicateClip (*c))
        {
            copy->setStart (te::TimePosition::fromSeconds (start + offset), false, true);
            copies.push_back (copy);
        }
    }

    clearSelection();
    selection = copies;

    if (! copies.empty())
        selectedClip = copies.back();

    save();
}

void EditSession::copySelectedClip()
{
    copiedClips = getSelectedClips();
}

void EditSession::pasteClip()
{
    std::vector<te::Clip::Ptr> sources;

    for (auto& c : copiedClips)
        if (c != nullptr && c->getClipTrack() != nullptr)
            sources.push_back (c);

    if (sources.empty() || getTransport().isRecording())
        return;

    double groupStart = 1.0e12;

    for (auto& c : sources)
        groupStart = juce::jmin (groupStart, c->getPosition().getStart().inSeconds());

    const double playhead = getTransport().getPosition().inSeconds();

    // One clip pastes onto the lane you last clicked; a group keeps its own tracks
    te::ClipTrack* target = getTrack (focusedTrack);
    const bool single = sources.size() == 1;

    beginEdit();
    std::vector<te::Clip::Ptr> pasted;

    for (auto& s : sources)
    {
        const double start = s->getPosition().getStart().inSeconds();
        auto copy = te::duplicateClip (*s);

        if (copy == nullptr)
            continue;

        if (single && target != nullptr && target != copy->getClipTrack())
            copy->moveTo (*target);

        copy->setStart (te::TimePosition::fromSeconds (playhead + (start - groupStart)), false, true);
        pasted.push_back (copy);
    }

    clearSelection();
    selection = pasted;

    if (! pasted.empty())
        selectedClip = pasted.back();

    save();
}
