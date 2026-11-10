#include "EditSession.h"

namespace te = tracktion;

static const juce::Identifier patternProp ("kngPattern");

static juce::String linkIdFor (int pattern, te::Track& track)
{
    return "kngpat" + juce::String (pattern) + "_" + track.itemID.toString();
}

static int patternOf (te::Clip& c)
{
    return (int) c.state.getProperty (patternProp, 0);
}

//==============================================================================
double EditSession::getBarSeconds()
{
    // Length of the bar under the playhead (bars can change length with the tempo map)
    const double start = barStartAt (getTransport().getPosition().inSeconds());
    return barsAfter (start, 1) - start;
}

int EditSession::getNumPatterns()
{
    int count = juce::jmax (1, (int) edit->state.getProperty ("kngPatternCount", 1));

    for (int i = 0; i < getNumTracks(); ++i)
        if (auto* t = getTrack (i))
            for (auto* c : t->getClips())
                count = juce::jmax (count, patternOf (*c));

    return count;
}

int EditSession::newPattern()
{
    const int next = getNumPatterns() + 1;
    edit->state.setProperty ("kngPatternCount", next, nullptr);
    setCurrentPattern (next);
    return next;
}

int EditSession::getPatternBars (int pattern)
{
    return juce::jlimit (1, 4, (int) edit->state.getProperty ("kngPatternBars" + juce::String (pattern), 1));
}

void EditSession::setPatternBars (int pattern, int bars)
{
    bars = juce::jlimit (1, 4, bars);

    beginEdit();
    edit->state.setProperty ("kngPatternBars" + juce::String (pattern), bars, nullptr);

    // Resize every copy of the pattern
    for (int i = 0; i < getNumTracks(); ++i)
        if (auto* t = getTrack (i))
            for (auto* c : t->getClips())
                if (auto* mc = dynamic_cast<te::MidiClip*> (c))
                    if (patternOf (*mc) == pattern)
                        mc->setEnd (te::TimePosition::fromSeconds (barsAfter (mc->getPosition().getStart().inSeconds(), bars)), true);

    save();
}

double EditSession::getPatternHome (int pattern)
{
    const juce::Identifier key ("kngPatternHome" + juce::String (pattern));

    if (edit->state.hasProperty (key))
        return (double) edit->state.getProperty (key);

    // Earliest copy on any track, otherwise the bar under the playhead
    double home = -1.0;

    for (int i = 0; i < getNumTracks(); ++i)
        if (auto* t = getTrack (i))
            for (auto* c : t->getClips())
                if (patternOf (*c) == pattern)
                {
                    const double start = c->getPosition().getStart().inSeconds();
                    home = home < 0.0 ? start : juce::jmin (home, start);
                }

    if (home < 0.0)
        home = barStartAt (getTransport().getPosition().inSeconds());

    edit->state.setProperty (key, home, nullptr);
    return home;
}

te::MidiClip* EditSession::getPatternClip (int trackIndex, int pattern, bool createIfMissing)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return nullptr;

    const double home = getPatternHome (pattern);
    te::MidiClip* first = nullptr;

    for (auto* c : track->getClips())
    {
        if (auto* mc = dynamic_cast<te::MidiClip*> (c))
        {
            if (patternOf (*mc) != pattern)
                continue;

            // Prefer the copy at the pattern's home
            if (std::abs (mc->getPosition().getStart().inSeconds() - home) < 0.001)
                return mc;

            if (first == nullptr)
                first = mc;
        }
    }

    if (first != nullptr || ! createIfMissing || getTransport().isRecording())
        return first;

    // If every copy was erased, bring the pattern back instead of starting empty
    if (auto* restored = restorePatternStash (trackIndex, pattern, home))
    {
        save();
        return restored;
    }

    beginEdit();

    const double length = barsAfter (home, getPatternBars (pattern)) - home;
    auto clip = track->insertMIDIClip (te::TimeRange (te::TimePosition::fromSeconds (home),
                                                      te::TimePosition::fromSeconds (home + length)),
                                       nullptr);

    if (clip == nullptr)
        return nullptr;

    clip->setName ("Pattern " + juce::String (pattern));
    clip->state.setProperty (patternProp, pattern, nullptr);
    clip->setLinkGroupID (linkIdFor (pattern, *track));
    clip->state.setProperty ("kngSwing", getPatternSwing (pattern), nullptr);

    save();
    return clip.get();
}

void EditSession::placePattern (int pattern, double atSeconds)
{
    if (getTransport().isRecording())
        return;

    const double start = barStartAt (atSeconds);

    beginEdit();

    for (int i = 0; i < getNumTracks(); ++i)
    {
        auto* track = getTrack (i);

        if (track == nullptr)
            continue;

        // Don't stack a copy on top of one that's already there
        bool alreadyThere = false;

        for (auto* c : track->getClips())
            if (patternOf (*c) == pattern && std::abs (c->getPosition().getStart().inSeconds() - start) < 0.001)
                alreadyThere = true;

        if (alreadyThere)
            continue;

        auto* master = getPatternClip (i, pattern, false);

        // Every copy was erased earlier: bring the pattern back from its stash
        if (master == nullptr)
        {
            if (auto* restored = restorePatternStash (i, pattern, start))
                if (restored->getLinkGroupID().isEmpty())
                    restored->setLinkGroupID (linkIdFor (pattern, *track));

            continue;
        }

        if (master->getLinkGroupID().isEmpty())
            master->setLinkGroupID (linkIdFor (pattern, *track));

        // The copy keeps the pattern number and link, so it stays in sync
        if (auto copy = te::duplicateClip (*master))
            copy->setStart (te::TimePosition::fromSeconds (start), false, true);
    }

    save();
}

void EditSession::loopPattern (int pattern)
{
    const double home = getPatternHome (pattern);
    const double length = barsAfter (home, getPatternBars (pattern)) - home;

    setLoopRange (home, home + length);
    setLooping (true);
    getTransport().setPosition (te::TimePosition::fromSeconds (home));

    if (! getTransport().isPlaying())
        play();
}

void EditSession::makePatternUnique (te::MidiClip& clip)
{
    const int oldPattern = patternOf (clip);

    if (oldPattern <= 0)
        return;

    const double start = clip.getPosition().getStart().inSeconds();
    const int fresh = getNumPatterns() + 1;

    beginEdit();

    edit->state.setProperty ("kngPatternCount", fresh, nullptr);
    edit->state.setProperty ("kngPatternHome" + juce::String (fresh), start, nullptr);
    edit->state.setProperty ("kngPatternBars" + juce::String (fresh), getPatternBars (oldPattern), nullptr);

    // Every track's copy at this spot becomes the new pattern
    for (int i = 0; i < getNumTracks(); ++i)
    {
        if (auto* track = getTrack (i))
        {
            for (auto* c : track->getClips())
            {
                if (patternOf (*c) == oldPattern && std::abs (c->getPosition().getStart().inSeconds() - start) < 0.001)
                {
                    c->state.setProperty (patternProp, fresh, &edit->getUndoManager());
                    c->setLinkGroupID (linkIdFor (fresh, *track));
                    c->setName ("Pattern " + juce::String (fresh));
                }
            }
        }
    }

    // The old pattern may have lost its home copy, so let it find a new one
    edit->state.removeProperty ("kngPatternHome" + juce::String (oldPattern), nullptr);

    setCurrentPattern (fresh);
    save();
}

//==============================================================================
// Swing: pushes every other 16th late, on sound rows only (never melodies)

float EditSession::getPatternSwing (int pattern)
{
    return (float) edit->state.getProperty ("kngPatternSwing" + juce::String (pattern), 0.0f);
}

void EditSession::setPatternSwing (int pattern, float percent)
{
    percent = juce::jlimit (0.0f, 100.0f, percent);
    edit->state.setProperty ("kngPatternSwing" + juce::String (pattern), percent, nullptr);

    const double stepBeats = 0.25;
    const double delay = percent / 100.0 * (stepBeats * 0.5);
    auto* um = &edit->getUndoManager();

    for (int i = 0; i < getNumTracks(); ++i)
    {
        auto* track = getTrack (i);

        if (track == nullptr || ! (bool) track->state.getProperty ("kngChannel", false))
            continue;

        for (auto* c : track->getClips())
        {
            auto* mc = dynamic_cast<te::MidiClip*> (c);

            if (mc == nullptr || patternOf (*mc) != pattern)
                continue;

            const double start = mc->getContentBeatAtTime (mc->getPosition().getStart()).inBeats();

            // Find each hit's step first (swing is under half a step, so flooring is safe)
            std::vector<std::pair<te::MidiNote*, int>> placed;

            for (auto* n : mc->getSequence().getNotes())
                placed.push_back ({ n, (int) std::floor ((n->getStartBeat().inBeats() - start + 0.001) / stepBeats) });

            mc->state.setProperty ("kngSwing", percent, um);

            for (auto& [n, step] : placed)
                n->setStartAndLength (te::BeatPosition::fromBeats (start + step * stepBeats + ((step % 2) == 1 ? delay : 0.0)),
                                      n->getLengthBeats(), um);
        }
    }
}
