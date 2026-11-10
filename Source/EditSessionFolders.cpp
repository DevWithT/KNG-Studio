#include "EditSession.h"

namespace te = tracktion;

static const juce::Identifier folderProp ("kngFolder");

static juce::Identifier collapsedKey (const juce::String& folder)
{
    return juce::Identifier ("kngFolderCollapsed_" + juce::String::toHexString (folder.hashCode()));
}

juce::String EditSession::getTrackFolder (int trackIndex)
{
    if (auto* t = getTrack (trackIndex))
        return t->state.getProperty (folderProp).toString();

    return {};
}

juce::Array<int> EditSession::getFolderTracks (const juce::String& folder)
{
    juce::Array<int> result;

    if (folder.isEmpty())
        return result;

    for (int i = 0; i < getNumTracks(); ++i)
        if (getTrackFolder (i) == folder)
            result.add (i);

    return result;
}

juce::StringArray EditSession::getFolderNames()
{
    juce::StringArray names;

    for (int i = 0; i < getNumTracks(); ++i)
    {
        const auto f = getTrackFolder (i);

        if (f.isNotEmpty())
            names.addIfNotAlreadyThere (f);
    }

    return names;
}

void EditSession::setTrackFolder (int trackIndex, const juce::String& folder)
{
    auto* t = getTrack (trackIndex);

    if (t == nullptr || getTransport().isRecording())
        return;

    beginEdit();

    const auto members = getFolderTracks (folder);
    t->state.setProperty (folderProp, folder, &edit->getUndoManager());

    // Move it next to the rest of its folder
    if (folder.isNotEmpty() && ! members.isEmpty() && ! members.contains (trackIndex))
    {
        int idx = trackIndex;
        const int first = members.getFirst();
        const int last = members.getLast();

        while (idx > last + 1)
            moveTrack (idx--, -1);

        while (idx < first - 1)
            moveTrack (idx++, 1);
    }

    save();
}

bool EditSession::isFolderCollapsed (const juce::String& folder)
{
    return (bool) edit->state.getProperty (collapsedKey (folder), false);
}

void EditSession::setFolderCollapsed (const juce::String& folder, bool collapsed)
{
    edit->state.setProperty (collapsedKey (folder), collapsed, nullptr);
}

void EditSession::renameFolder (const juce::String& from, const juce::String& to)
{
    if (to.isEmpty() || from == to)
        return;

    const bool collapsed = isFolderCollapsed (from);
    beginEdit();

    for (int i : getFolderTracks (from))
        if (auto* t = getTrack (i))
            t->state.setProperty (folderProp, to, &edit->getUndoManager());

    setFolderCollapsed (to, collapsed);
    save();
}

void EditSession::removeFolder (const juce::String& folder)
{
    beginEdit();

    for (int i : getFolderTracks (folder))
        if (auto* t = getTrack (i))
            t->state.removeProperty (folderProp, &edit->getUndoManager());

    edit->state.removeProperty (collapsedKey (folder), nullptr);
    save();
}

void EditSession::toggleFolderMute (const juce::String& folder)
{
    const auto members = getFolderTracks (folder);
    bool anyUnmuted = false;

    for (int i : members)
        if (auto* t = getTrack (i))
            anyUnmuted = anyUnmuted || ! t->isMuted (false);

    for (int i : members)
        if (auto* t = getTrack (i))
            t->setMute (anyUnmuted);
}

void EditSession::toggleFolderSolo (const juce::String& folder)
{
    const auto members = getFolderTracks (folder);
    bool anyUnsoloed = false;

    for (int i : members)
        if (auto* t = getTrack (i))
            anyUnsoloed = anyUnsoloed || ! t->isSolo (false);

    for (int i : members)
        if (auto* t = getTrack (i))
            t->setSolo (anyUnsoloed);
}
