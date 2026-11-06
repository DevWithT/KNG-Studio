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
    if (clip == nullptr)
        return false;

    if (selectedClip.get() == clip)
        return true;

    for (auto& s : selection)
        if (s.get() == clip)
            return true;

    return false;
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

    return result;
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
