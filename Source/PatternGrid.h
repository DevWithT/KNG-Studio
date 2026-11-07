#pragma once

#include <tracktion_engine/tracktion_engine.h>

/** Step-grid helpers shared by the Rack and the drum sequencer. Steps are 16th notes. */
namespace PatternGrid
{
    constexpr double stepBeats = 0.25;

    inline double clipStartBeat (tracktion::MidiClip& c)
    {
        return c.getContentBeatAtTime (c.getPosition().getStart()).inBeats();
    }

    inline double swingDelay (tracktion::MidiClip& c)
    {
        return (double) c.state.getProperty ("kngSwing", 0.0) / 100.0 * (stepBeats * 0.5);
    }

    inline double stepToBeat (tracktion::MidiClip& c, int step)
    {
        return step * stepBeats + ((step % 2) == 1 ? swingDelay (c) : 0.0);
    }

    inline int beatToStep (double beatFromClipStart)
    {
        return (int) std::floor ((beatFromClipStart + 0.001) / stepBeats);
    }

    inline tracktion::MidiNote* findNote (tracktion::MidiClip& c, int noteNumber, int step)
    {
        const double start = clipStartBeat (c);

        for (auto* n : c.getSequence().getNotes())
            if (n->getNoteNumber() == noteNumber && beatToStep (n->getStartBeat().inBeats() - start) == step)
                return n;

        return nullptr;
    }

    inline void setStep (tracktion::MidiClip& c, int noteNumber, int step, bool on, juce::UndoManager* um)
    {
        auto* existing = findNote (c, noteNumber, step);

        if (on && existing == nullptr)
            c.getSequence().addNote (noteNumber,
                                     tracktion::BeatPosition::fromBeats (clipStartBeat (c) + stepToBeat (c, step)),
                                     tracktion::BeatDuration::fromBeats (stepBeats),
                                     100, 0, um);
        else if (! on && existing != nullptr)
            c.getSequence().removeNote (*existing, um);
    }
}
