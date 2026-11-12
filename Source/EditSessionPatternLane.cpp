#include "EditSession.h"

namespace te = tracktion;

static int blockPatternOf (te::Clip& c)
{
    return (int) c.state.getProperty ("kngPattern", 0);
}

//==============================================================================
std::vector<EditSession::PatternBlock> EditSession::getPatternBlocks()
{
    std::vector<PatternBlock> blocks;
    const double bankStart = getBankStart() - 0.001;
    const auto defaultLane = getDefaultLaneId();

    for (int i = 0; i < getNumTracks(); ++i)
    {
        if (auto* track = getTrack (i))
        {
            for (auto* c : track->getClips())
            {
                const int p = blockPatternOf (*c);

                if (p <= 0)
                    continue;

                const double s = c->getPosition().getStart().inSeconds();
                const double e = c->getPosition().getEnd().inSeconds();
                bool merged = false;

                // Bank copies aren't part of the song
                if (s >= bankStart)
                    continue;

                // Which lane this copy sits in (old copies and copies from deleted lanes go in the first lane)
                auto lane = c->state.getProperty ("kngLane").toString();

                if (getLaneTrackIndex (lane) < 0)
                    lane = defaultLane;

                // Copies of the same pattern at the same spot in the same lane are one block
                for (auto& b : blocks)
                {
                    if (b.pattern == p && b.lane == lane && std::abs (b.start - s) < 0.001)
                    {
                        b.end = juce::jmax (b.end, e);
                        merged = true;
                        break;
                    }
                }

                if (! merged)
                    blocks.push_back ({ p, s, e, lane });
            }
        }
    }

    return blocks;
}

void EditSession::removePatternAt (double seconds)
{
    if (getTransport().isRecording())
        return;

    int pattern = 0;
    double start = -1.0;

    for (auto& b : getPatternBlocks())
    {
        if (seconds >= b.start && seconds < b.end)
        {
            pattern = b.pattern;
            start = b.start;
            break;
        }
    }

    if (pattern <= 0)
        return;

    beginEdit();

    for (int i = 0; i < getNumTracks(); ++i)
    {
        auto* track = getTrack (i);

        if (track == nullptr)
            continue;

        juce::Array<te::Clip*> here;
        int copiesOnTrack = 0;

        for (auto* c : track->getClips())
        {
            if (blockPatternOf (*c) != pattern)
                continue;

            ++copiesOnTrack;

            if (std::abs (c->getPosition().getStart().inSeconds() - start) < 0.001)
                here.add (c);
        }

        for (auto* c : here)
        {
            // Last copy on this track: keep its notes so painting it again brings them back
            if (copiesOnTrack == 1)
                track->state.setProperty ("kngStash" + juce::String (pattern), c->state.toXmlString(), nullptr);

            --copiesOnTrack;
            c->removeFromParent();
        }
    }

    // The pattern's home copy may be gone, so let it find a new one
    edit->state.removeProperty ("kngPatternHome" + juce::String (pattern), nullptr);
    save();
}

te::MidiClip* EditSession::restorePatternStash (int trackIndex, int pattern, double atSeconds)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return nullptr;

    const juce::Identifier key ("kngStash" + juce::String (pattern));
    const auto xml = track->state.getProperty (key).toString();

    if (xml.isEmpty())
        return nullptr;

    auto clipState = juce::ValueTree::fromXml (xml);

    if (! clipState.isValid())
        return nullptr;

    edit->createNewItemID().writeID (clipState, nullptr);

    auto inserted = track->insertClipWithState (clipState);

    if (inserted == nullptr)
        return nullptr;

    auto* clip = dynamic_cast<te::MidiClip*> (&*inserted);

    if (clip == nullptr)
        return nullptr;

    clip->setStart (te::TimePosition::fromSeconds (atSeconds), false, true);
    track->state.removeProperty (key, nullptr);
    return clip;
}
