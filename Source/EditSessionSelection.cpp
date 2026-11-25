#include "EditSession.h"

namespace te = tracktion;

void EditSession::clearSelection()
{
    selectedClip = nullptr;
    selection.clear();
}

void EditSession::toggleClipSelection (te::Clip* clip)
{
    if (clip == nullptr)
        return;

    for (auto it = selection.begin(); it != selection.end(); ++it)
    {
        if (it->get() == clip)
        {
            selection.erase (it);

            if (selectedClip.get() == clip)
                selectedClip = selection.empty() ? nullptr : selection.back();

            return;
        }
    }

    selection.push_back (clip);
    selectedClip = clip;
}

void EditSession::addClipsToSelection (const juce::Array<te::Clip*>& clips)
{
    for (auto* c : clips)
        if (! isClipSelected (c))
            selection.push_back (c);

    if (! clips.isEmpty())
        selectedClip = clips.getLast();
}

bool EditSession::isClipSelected (te::Clip* clip) const
{
    if (isClipSelectedDirect (clip))
        return true;

    // A clip counts as selected when someone in its group is
    const auto id = clip != nullptr ? clip->state.getProperty ("kngGroup").toString() : juce::String();

    if (id.isEmpty())
        return false;

    for (auto& s : selection)
        if (s != nullptr && s->state.getProperty ("kngGroup").toString() == id)
            return true;

    return selectedClip != nullptr && selectedClip->state.getProperty ("kngGroup").toString() == id;
}

bool EditSession::isClipSelectedDirect (te::Clip* clip) const
{
    if (clip == nullptr)
        return false;

    if (selectedClip.get() == clip)
        return true;

    for (auto& s : selection)
        if (s.get() == clip)
            return true;

    return false;
}

// Group Clips: adds every clip that shares a group with a clip already in the list
static void addGroupMates (EditSession& s, std::vector<te::Clip::Ptr>& result)
{
    juce::StringArray ids;

    for (auto& r : result)
    {
        const auto id = r->state.getProperty ("kngGroup").toString();

        if (id.isNotEmpty())
            ids.addIfNotAlreadyThere (id);
    }

    if (ids.isEmpty())
        return;

    for (int i = 0; i < s.getNumTracks(); ++i)
    {
        auto* t = s.getTrack (i);

        if (t == nullptr)
            continue;

        for (auto* c : t->getClips())
        {
            if (! ids.contains (c->state.getProperty ("kngGroup").toString()))
                continue;

            bool found = false;

            for (auto& r : result)
                found = found || r.get() == c;

            if (! found)
                result.push_back (c);
        }
    }
}

std::vector<te::Clip::Ptr> EditSession::getSelectedClips() const
{
    std::vector<te::Clip::Ptr> result;

    // Skip anything that's been deleted since it was selected
    for (auto& s : selection)
        if (s != nullptr && s->getParent() != nullptr)
            result.push_back (s);

    if (selectedClip != nullptr && selectedClip->getParent() != nullptr)
    {
        bool found = false;

        for (auto& r : result)
            found = found || r == selectedClip;

        if (! found)
            result.push_back (selectedClip);
    }

    addGroupMates (const_cast<EditSession&> (*this), result);

    return result;
}

bool EditSession::isClipGrouped (te::Clip* clip) const
{
    return clip != nullptr && clip->state.getProperty ("kngGroup").toString().isNotEmpty();
}

void EditSession::groupSelectedClips()
{
    const auto clips = getSelectedClips();   // already includes everyone in a group that is selected

    if (clips.size() < 2 || getTransport().isRecording())
        return;

    beginEdit ("Group clips");
    const auto id = juce::Uuid().toString();

    for (auto& c : clips)
        c->state.setProperty ("kngGroup", id, &edit->getUndoManager());

    save();
}

void EditSession::ungroupSelectedClips()
{
    const auto clips = getSelectedClips();
    bool any = false;

    for (auto& c : clips)
        any = any || isClipGrouped (c.get());

    if (! any)
        return;

    beginEdit ("Ungroup clips");

    for (auto& c : clips)
        c->state.removeProperty ("kngGroup", &edit->getUndoManager());

    save();
}

// A copy of a grouped clip must not join the original's group: each group of copies gets a fresh id
void EditSession::regroupCopies (const std::vector<te::Clip::Ptr>& copies)
{
    juce::StringArray oldIds, newIds;

    for (auto& c : copies)
    {
        const auto id = c->state.getProperty ("kngGroup").toString();

        if (id.isEmpty())
            continue;

        int k = oldIds.indexOf (id);

        if (k < 0)
        {
            oldIds.add (id);
            newIds.add (juce::Uuid().toString());
            k = oldIds.size() - 1;
        }

        c->state.setProperty ("kngGroup", newIds[k], &edit->getUndoManager());
    }
}

void EditSession::selectAllClips()
{
    clearSelection();

    for (int i = 0; i < getNumTracks(); ++i)
        if (auto* t = getTrack (i))
            for (auto* c : t->getClips())
                selection.push_back (c);

    if (! selection.empty())
        selectedClip = selection.back();
}
