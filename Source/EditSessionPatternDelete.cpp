#include "EditSession.h"

namespace te = tracktion;

static const juce::Identifier patternProp ("kngPattern");

static juce::String linkFor (int pattern, te::Track& track)
{
    return "kngpat" + juce::String (pattern) + "_" + track.itemID.toString();
}

// Moves a saved setting from one pattern number to another (or clears it if the source has none)
static void moveKey (juce::ValueTree& state, const juce::String& base, int from, int to, juce::UndoManager* um)
{
    const juce::Identifier src (base + juce::String (from)), dst (base + juce::String (to));

    if (state.hasProperty (src))
        state.setProperty (dst, state.getProperty (src), um);
    else
        state.removeProperty (dst, um);
}

bool EditSession::deletePattern (int pattern)
{
    const int count = getNumPatterns();

    if (getTransport().isRecording() || pattern < 1 || pattern > count)
        return false;

    beginEdit();
    auto* um = &edit->getUndoManager();
    auto& st = edit->state;

    // 1. Every copy of this pattern goes: its bank home, every block, and its stash
    for (int i = 0; i < getNumTracks(); ++i)
    {
        auto* track = getTrack (i);

        if (track == nullptr)
            continue;

        juce::Array<te::Clip*> doomed;

        for (auto* c : track->getClips())
            if ((int) c->state.getProperty (patternProp, 0) == pattern)
                doomed.add (c);

        for (auto* c : doomed)
            c->removeFromParent();

        track->state.removeProperty ("kngStash" + juce::String (pattern), um);
    }

    // 2. Every later pattern moves down one number: its clips, links, bank slot, stash and settings
    for (int q = pattern + 1; q <= count; ++q)
    {
        const double oldHome = getPatternHome (q);
        const double newHome = getPatternHome (q - 1);

        for (int i = 0; i < getNumTracks(); ++i)
        {
            auto* track = getTrack (i);

            if (track == nullptr)
                continue;

            const auto oldLink = linkFor (q, *track);
            const auto newLink = linkFor (q - 1, *track);

            for (auto* c : track->getClips())
            {
                if ((int) c->state.getProperty (patternProp, 0) != q)
                    continue;

                c->state.setProperty (patternProp, q - 1, um);

                if (c->getLinkGroupID() == oldLink)
                    c->setLinkGroupID (newLink);

                // The bank copy moves into the slot below
                if (std::abs (c->getPosition().getStart().inSeconds() - oldHome) < 0.001)
                    c->setStart (te::TimePosition::fromSeconds (newHome), false, true);
            }

            // The stash keeps the clip as text: renumber what's inside it too
            const juce::Identifier oldStash ("kngStash" + juce::String (q)), newStash ("kngStash" + juce::String (q - 1));

            if (track->state.hasProperty (oldStash))
            {
                auto clipState = juce::ValueTree::fromXml (track->state.getProperty (oldStash).toString());

                if (clipState.isValid())
                {
                    clipState.setProperty (patternProp, q - 1, nullptr);

                    for (int k = 0; k < clipState.getNumProperties(); ++k)
                    {
                        const auto name = clipState.getPropertyName (k);

                        if (clipState.getProperty (name).toString() == oldLink)
                            clipState.setProperty (name, newLink, nullptr);
                    }

                    track->state.setProperty (newStash, clipState.toXmlString(), um);
                }

                track->state.removeProperty (oldStash, um);
            }
        }

        for (auto* base : { "kngPatternName", "kngPatternColour", "kngPatternBars", "kngPatternSwing", "kngPatternHome" })
            moveKey (st, base, q, q - 1, um);
    }

    // 3. The last number is now free
    for (auto* base : { "kngPatternName", "kngPatternColour", "kngPatternBars", "kngPatternSwing", "kngPatternHome" })
        st.removeProperty (juce::String (base) + juce::String (count), um);

    st.setProperty ("kngPatternCount", juce::jmax (1, count - 1), um);

    // Keep the same pattern selected if it moved, otherwise the one before
    const int current = getCurrentPattern();

    if (current > pattern)
        setCurrentPattern (current - 1);
    else if (current == pattern)
        setCurrentPattern (juce::jmax (1, pattern - 1));

    save();
    return true;
}
