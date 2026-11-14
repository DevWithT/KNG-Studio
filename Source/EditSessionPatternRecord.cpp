#include "EditSession.h"

namespace te = tracktion;

// Notes recorded while a pattern looped go into that pattern (every copy updates), and the take itself goes away
void EditSession::mergePatternTake (int pattern)
{
    auto& ts = edit->tempoSequence;
    const double home = getPatternHome (pattern);
    const double homeBeat = ts.toBeats (te::TimePosition::fromSeconds (home)).inBeats();
    const double lengthBeats = ts.toBeats (te::TimePosition::fromSeconds (barsAfter (home, getPatternBars (pattern)))).inBeats() - homeBeat;

    if (lengthBeats <= 0.0)
        return;

    auto* um = &edit->getUndoManager();
    bool changed = false;

    for (int i = 0; i < getNumTracks(); ++i)
    {
        auto* track = getTrack (i);

        if (track == nullptr || ! isMidiTrack (i))
            continue;

        juce::Array<te::MidiClip*> fresh;

        for (auto* c : track->getClips())
            if (auto* mc = dynamic_cast<te::MidiClip*> (c))
                if (std::find (clipsBeforeRecording.begin(), clipsBeforeRecording.end(), c->itemID) == clipsBeforeRecording.end())
                    fresh.add (mc);

        if (fresh.isEmpty())
            continue;

        if (! changed)
        {
            beginEdit();
            changed = true;
        }

        auto* target = getPatternClip (i, pattern, true);

        for (auto* take : fresh)
        {
            if (take == target)
                continue;

            if (target != nullptr)
            {
                const double takeStartBeat = ts.toBeats (take->getPosition().getStart()).inBeats();
                const double takeContent = take->getContentBeatAtTime (take->getPosition().getStart()).inBeats();
                const double targetContent = target->getContentBeatAtTime (target->getPosition().getStart()).inBeats();

                for (auto* n : take->getSequence().getNotes())
                {
                    // Where the note fell inside the pattern, wrapping each loop pass back to the start
                    double b = takeStartBeat - homeBeat + (n->getStartBeat().inBeats() - takeContent);
                    b = std::fmod (b, lengthBeats);

                    if (b < 0.0)
                        b += lengthBeats;

                    const double len = juce::jmax (0.05, juce::jmin (n->getLengthBeats().inBeats(), lengthBeats - b));

                    target->getSequence().addNote (n->getNoteNumber(), te::BeatPosition::fromBeats (targetContent + b),
                                                   te::BeatDuration::fromBeats (len), n->getVelocity(), 0, um);
                }
            }

            take->removeFromParent();
        }

        if (target != nullptr && target->isLinked())
            target->updateLinkedClips();
    }

    if (changed)
        save();
}
