#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
// Sections: a marker with a length and a "kngSection" property. The marker row draws it as a named block.
// Moving a section moves the clips that start inside it. Duplicating one makes room first.

static void makeSectionMarker (te::Edit& edit, double start, double length, const juce::String& name)
{
    edit.ensureMarkerTrack();

    auto& mm = edit.getMarkerManager();
    auto marker = mm.createMarker (mm.getNextUniqueID(), te::TimePosition::fromSeconds (start),
                                   te::TimeDuration::fromSeconds (length), nullptr);

    if (marker != nullptr)
    {
        marker->setName (name);
        marker->state.setProperty ("kngSection", true, &edit.getUndoManager());
    }
}

void EditSession::addSection (double startSeconds, const juce::String& name, double lengthSeconds)
{
    beginEdit ("Add section");
    makeSectionMarker (*edit, juce::jmax (0.0, startSeconds), juce::jmax (0.1, lengthSeconds), name);
    save();
}

std::vector<te::Clip::Ptr> EditSession::getSectionClips (double fromSeconds, double toSeconds)
{
    std::vector<te::Clip::Ptr> result;

    for (int i = 0; i < getNumTracks(); ++i)
    {
        if (auto* t = getTrack (i))
        {
            for (auto* c : t->getClips())
            {
                const double s = c->getPosition().getStart().inSeconds();

                if (s >= fromSeconds - 0.0005 && s < toSeconds - 0.0005)
                    result.push_back (c);
            }
        }
    }

    return result;
}

void EditSession::moveClipsInRange (double fromSeconds, double toSeconds, double deltaSeconds)
{
    const auto clips = getSectionClips (fromSeconds, toSeconds);

    for (auto& c : clips)
        c->setStart (te::TimePosition::fromSeconds (juce::jmax (0.0, c->getPosition().getStart().inSeconds() + deltaSeconds)),
                     false, true);
}

int EditSession::duplicateSection (te::Clip& section)
{
    if (getTransport().isRecording())
        return -1;

    const double start = section.getPosition().getStart().inSeconds();
    const double end = section.getPosition().getEnd().inSeconds();
    const double length = end - start;

    if (length < 0.05)
        return -1;

    const auto content = getSectionClips (start, end);
    const auto name = section.getName();

    beginEdit ("Duplicate section");

    // Make room: everything that starts at or after the end moves right by the section's length
    for (int i = 0; i < getNumTracks(); ++i)
    {
        if (auto* t = getTrack (i))
        {
            for (auto* c : t->getClips())
            {
                const double s = c->getPosition().getStart().inSeconds();

                if (s >= end - 0.0005)
                    c->setStart (te::TimePosition::fromSeconds (s + length), false, true);
            }
        }
    }

    for (auto& m : getMarkers())
    {
        const double s = m->getPosition().getStart().inSeconds();

        if (m.get() != &section && s >= end - 0.0005)
            m->setStart (te::TimePosition::fromSeconds (s + length), false, true);
    }

    // The copies
    std::vector<te::Clip::Ptr> copies;

    for (auto& c : content)
    {
        const double s = c->getPosition().getStart().inSeconds();

        if (auto copy = te::duplicateClip (*c))
        {
            copy->setStart (te::TimePosition::fromSeconds (s + length), false, true);
            copies.push_back (copy);
        }
    }

    regroupCopies (copies);

    // A fresh name: Verse, Verse 2, Verse 3
    auto base = name.trimEnd();

    while (base.isNotEmpty() && juce::CharacterFunctions::isDigit (base.getLastCharacter()))
        base = base.dropLastCharacters (1);

    base = base.trimEnd();

    if (base.isEmpty())
        base = "Section";

    int n = 2;

    for (;;)
    {
        bool used = false;

        for (auto& m : getMarkers())
            used = used || m->getName() == base + " " + juce::String (n);

        if (! used)
            break;

        ++n;
    }

    makeSectionMarker (*edit, end, length, base + " " + juce::String (n));
    save();

    return (int) copies.size();
}
