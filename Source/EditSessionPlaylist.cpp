#include "EditSession.h"

namespace te = tracktion;

static const juce::Identifier laneProp ("kngLane");

static int clipPattern (te::Clip& c)
{
    return (int) c.state.getProperty ("kngPattern", 0);
}

//==============================================================================
bool EditSession::isLaneTrack (int trackIndex)
{
    return getTrack (trackIndex) != nullptr && ! isMidiTrack (trackIndex) && ! isReverbBus (trackIndex);
}

juce::String EditSession::getLaneId (int trackIndex)
{
    if (auto* t = getTrack (trackIndex))
        return t->itemID.toString();

    return {};
}

int EditSession::getLaneTrackIndex (const juce::String& laneId)
{
    for (int i = 0; i < getNumTracks(); ++i)
        if (isLaneTrack (i) && getLaneId (i) == laneId)
            return i;

    return -1;
}

juce::String EditSession::getDefaultLaneId()
{
    for (int i = 0; i < getNumTracks(); ++i)
        if (isLaneTrack (i))
            return getLaneId (i);

    return {};
}

juce::Array<te::Clip*> EditSession::getBlockClips (const PatternBlock& block)
{
    juce::Array<te::Clip*> result;
    const auto defaultLane = getDefaultLaneId();

    for (int i = 0; i < getNumTracks(); ++i)
    {
        if (auto* t = getTrack (i))
        {
            for (auto* c : t->getClips())
            {
                if (clipPattern (*c) != block.pattern
                     || std::abs (c->getPosition().getStart().inSeconds() - block.start) > 0.001)
                    continue;

                auto lane = c->state.getProperty (laneProp).toString();

                if (getLaneTrackIndex (lane) < 0)
                    lane = defaultLane;

                if (lane == block.lane)
                    result.add (c);
            }
        }
    }

    return result;
}

//==============================================================================
void EditSession::placePatternInLane (int pattern, double atSeconds, const juce::String& laneId)
{
    if (getTransport().isRecording())
        return;

    const double start = barStartAt (juce::jmax (0.0, atSeconds));

    // A copy already here (in any lane) would play twice
    for (auto& b : getPatternBlocks())
        if (b.pattern == pattern && std::abs (b.start - start) < 0.001)
            return;

    // An empty pattern still needs a clip on one Rack sound so its block shows up
    bool hasClip = false;

    for (int i = 0; i < getNumTracks() && ! hasClip; ++i)
        hasClip = getPatternClip (i, pattern, false) != nullptr;

    if (! hasClip)
    {
        for (int i = 0; i < getNumTracks(); ++i)
        {
            if (isMidiTrack (i) && ! isDrumTrack (i))
            {
                getPatternClip (i, pattern, true);
                break;
            }
        }
    }

    placePattern (pattern, start);

    for (int i = 0; i < getNumTracks(); ++i)
        if (auto* t = getTrack (i))
            for (auto* c : t->getClips())
                if (clipPattern (*c) == pattern && ! c->state.hasProperty (laneProp)
                     && std::abs (c->getPosition().getStart().inSeconds() - start) < 0.001)
                    c->state.setProperty (laneProp, laneId, nullptr);

    save();
}

void EditSession::moveBlock (const PatternBlock& block, double toSeconds, const juce::String& toLane, bool copy)
{
    if (getTransport().isRecording())
        return;

    const double to = barStartAt (juce::jmax (0.0, toSeconds));
    const bool samePlace = std::abs (to - block.start) < 0.001 && toLane == block.lane;

    if (samePlace)
        return;

    beginEdit();

    for (auto* c : getBlockClips (block))
    {
        te::Clip* target = c;
        te::Clip::Ptr dup;

        if (copy)
        {
            dup = te::duplicateClip (*c);

            if (dup == nullptr)
                continue;

            target = dup.get();
        }

        target->setStart (te::TimePosition::fromSeconds (to), false, true);
        target->state.setProperty (laneProp, toLane, nullptr);
    }

    save();
}

void EditSession::deleteBlock (const PatternBlock& block)
{
    if (getTransport().isRecording())
        return;

    // Keep the pattern's notes safe in the bank before this copy goes
    placePattern (block.pattern, getPatternHome (block.pattern));

    beginEdit();

    for (auto* c : getBlockClips (block))
        c->removeFromParent();

    save();
}

//==============================================================================
void EditSession::cuePattern (int pattern, bool startPlaying)
{
    if (getTransport().isRecording())
        return;

    placePattern (pattern, getPatternHome (pattern));

    const double home = getPatternHome (pattern);
    setLoopRange (home, barsAfter (home, getPatternBars (pattern)));
    setLooping (true);
    patternMode = true;

    getTransport().setPosition (te::TimePosition::fromSeconds (home));

    if (startPlaying && ! getTransport().isPlaying())
        play();
}

void EditSession::setPatternMode (bool shouldLoopPattern)
{
    if (shouldLoopPattern)
    {
        cuePattern (currentPattern, getTransport().isPlaying());
        return;
    }

    patternMode = false;
    setLooping (false);

    // Back to the song if the playhead was in the bank
    if (getTransport().getPosition().inSeconds() >= getBankStart() - 1.0)
        getTransport().setPosition (te::TimePosition());
}
