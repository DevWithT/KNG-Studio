#pragma once

#include <vector>
#include "EditSession.h"

// Cut lists for the Slice Editor. Every cut is in seconds from the clip's start, between the two ends.
namespace KngSliceCuts
{
    struct GridChoice { const char* name; double beats; };

    // Grid sizes in beats (1 = quarter note). T means triplets.
    inline constexpr GridChoice gridChoices[] = { { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/8T", 1.0 / 3.0 },
                                                  { "1/16", 0.25 }, { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 } };

    // A cut at every grid line after the clip's start, counted on the song's tempo map. Stops after maxCuts.
    inline std::vector<double> grid (EditSession& session, tracktion::AudioClipBase& clip, double beats, int maxCuts)
    {
        std::vector<double> result;

        if (beats <= 0.0)
            return result;

        auto& ts = session.getEdit().tempoSequence;
        const auto pos = clip.getPosition();
        const double start = pos.getStart().inSeconds();
        const double length = pos.getLength().inSeconds();
        const double startBeat = ts.toBeats (tracktion::TimePosition::fromSeconds (start)).inBeats();
        const double endBeat = ts.toBeats (tracktion::TimePosition::fromSeconds (start + length)).inBeats();

        for (int k = 1; k <= maxCuts; ++k)
        {
            const double beat = startBeat + k * beats;

            if (beat >= endBeat - 0.001)
                break;

            const double t = ts.toTime (tracktion::BeatPosition::fromBeats (beat)).inSeconds() - start;

            if (t > 0.01 && t < length - 0.01)
                result.push_back (t);
        }

        return result;
    }

    // Equal parts: parts - 1 cuts.
    inline std::vector<double> equalParts (double length, int parts)
    {
        std::vector<double> result;
        parts = juce::jlimit (1, 16, parts);

        for (int i = 1; i < parts; ++i)
            result.push_back (length * i / parts);

        return result;
    }
}
