#include "EditSession.h"
#include "PatternGrid.h"
#include <algorithm>
#include <map>

namespace te = tracktion;

static void finish (te::MidiClip& clip)
{
    if (clip.isLinked())
        clip.updateLinkedClips();
}

//==============================================================================
// Any clip (the Piano Roll uses these directly)

void EditSession::humanizeClip (te::MidiClip& clip, double maxShiftBeats, int velocityRange)
{
    if (getTransport().isRecording() || clip.getSequence().getNumNotes() == 0)
        return;

    auto* um = &edit->getUndoManager();
    const double start = PatternGrid::clipStartBeat (clip);
    juce::Random random;

    beginEdit();

    for (auto* n : clip.getSequence().getNotes())
    {
        const double shift = (random.nextDouble() * 2.0 - 1.0) * maxShiftBeats;
        const double newStart = juce::jmax (start, n->getStartBeat().inBeats() + shift);
        n->setStartAndLength (te::BeatPosition::fromBeats (newStart), n->getLengthBeats(), um);

        const int v = n->getVelocity() + random.nextInt (velocityRange * 2 + 1) - velocityRange;
        n->setVelocity (juce::jlimit (1, 127, v), um);
    }

    finish (clip);
    save();
}

void EditSession::arpeggiateClip (te::MidiClip& clip, int direction, double stepBeats)
{
    if (getTransport().isRecording() || stepBeats <= 0.0)
        return;

    auto* um = &edit->getUndoManager();
    auto& seq = clip.getSequence();

    // Notes that start together (within a 64th) make a chord
    std::map<long long, juce::Array<te::MidiNote*>> chords;

    for (auto* n : seq.getNotes())
        chords[(long long) std::llround (n->getStartBeat().inBeats() * 16.0)].add (n);

    struct NewNote { int pitch; double start, length; int velocity; };
    std::vector<NewNote> added;
    juce::Array<te::MidiNote*> removed;

    for (auto& [key, notes] : chords)
    {
        if (notes.size() < 2)
            continue;

        juce::Array<int> pitches;
        double chordStart = 1.0e9, chordEnd = 0.0;
        int velocity = 0;

        for (auto* n : notes)
        {
            pitches.addIfNotAlreadyThere (n->getNoteNumber());
            chordStart = juce::jmin (chordStart, n->getStartBeat().inBeats());
            chordEnd = juce::jmax (chordEnd, n->getEndBeat().inBeats());
            velocity = juce::jmax (velocity, n->getVelocity());
            removed.add (n);
        }

        pitches.sort();
        juce::Array<int> order;

        if (direction == 1)
        {
            for (int k = pitches.size(); --k >= 0;)
                order.add (pitches[k]);
        }
        else
        {
            order = pitches;

            // Up-down: up, then back down without repeating the top and bottom notes
            if (direction == 2)
                for (int k = pitches.size() - 2; k >= 1; --k)
                    order.add (pitches[k]);
        }

        int step = 0;

        for (double b = chordStart; b < chordEnd - 1.0e-6; b += stepBeats, ++step)
            added.push_back ({ order[step % order.size()], b, juce::jmin (stepBeats * 0.9, chordEnd - b), velocity });
    }

    if (removed.isEmpty())
        return;

    beginEdit();

    for (auto* n : removed)
        seq.removeNote (*n, um);

    for (auto& a : added)
        seq.addNote (a.pitch, te::BeatPosition::fromBeats (a.start), te::BeatDuration::fromBeats (juce::jmax (0.02, a.length)),
                     a.velocity, 0, um);

    finish (clip);
    save();
}

void EditSession::setClipNoteLength (te::MidiClip& clip, double beats)
{
    if (getTransport().isRecording())
        return;

    auto* um = &edit->getUndoManager();
    const auto notes = clip.getSequence().getNotes();

    if (notes.isEmpty())
        return;

    // Legato runs to the end of the clip at most
    const double clipEnd = clip.getContentBeatAtTime (clip.getPosition().getEnd()).inBeats();

    // Every distinct start, so legato can find the next note of any pitch
    std::vector<double> starts;

    for (auto* n : notes)
        starts.push_back (n->getStartBeat().inBeats());

    std::sort (starts.begin(), starts.end());

    beginEdit();

    for (auto* n : notes)
    {
        const double s = n->getStartBeat().inBeats();
        double length = beats;

        if (beats <= 0.0)
        {
            auto next = std::upper_bound (starts.begin(), starts.end(), s + 1.0e-4);
            length = (next != starts.end() ? *next : clipEnd) - s;
        }

        n->setStartAndLength (n->getStartBeat(), te::BeatDuration::fromBeats (juce::jmax (0.02, length)), um);
    }

    finish (clip);
    save();
}

//==============================================================================
// One sound's notes in the current pattern (the Rack row menu)

void EditSession::humanizePattern (int trackIndex, double maxShiftBeats, int velocityRange)
{
    if (auto* clip = getPatternClip (trackIndex, currentPattern, false))
        humanizeClip (*clip, maxShiftBeats, velocityRange);
}

void EditSession::arpeggiatePattern (int trackIndex, int direction, double stepBeats)
{
    if (auto* clip = getPatternClip (trackIndex, currentPattern, false))
        arpeggiateClip (*clip, direction, stepBeats);
}

void EditSession::setPatternNoteLength (int trackIndex, double beats)
{
    if (auto* clip = getPatternClip (trackIndex, currentPattern, false))
        setClipNoteLength (*clip, beats);
}
