#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <vector>

/** Small music theory toolkit for the piano roll. Middle C (MIDI 60) is C4. */
namespace Theory
{
    inline const juce::StringArray& noteNames()
    {
        static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names;
    }

    inline int wrap12 (int n)                       { return ((n % 12) + 12) % 12; }
    inline juce::String pitchName (int pitchClass)  { return noteNames()[wrap12 (pitchClass)]; }
    inline juce::String noteName (int midiNote)     { return pitchName (midiNote) + juce::String (midiNote / 12 - 1); }
    inline int pitchClassFrom (int note, int root)  { return wrap12 (note - root); }

    inline bool isBlackKey (int note)
    {
        const int n = wrap12 (note);
        return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
    }

    inline const juce::StringArray& intervalNames()
    {
        static const juce::StringArray names { "Root", "Minor 2nd", "Major 2nd", "Minor 3rd", "Major 3rd", "Perfect 4th",
                                               "Tritone", "Perfect 5th", "Minor 6th", "Major 6th", "Minor 7th", "Major 7th" };
        return names;
    }

    //==============================================================================
    struct Scale
    {
        juce::String name;
        std::vector<int> steps;
        juce::String about;
    };

    inline const std::vector<Scale>& scales()
    {
        static const std::vector<Scale> list {
            { "Major",            { 0, 2, 4, 5, 7, 9, 11 }, "Bright and happy. The pattern is whole, whole, half, whole, whole, whole, half." },
            { "Natural Minor",    { 0, 2, 3, 5, 7, 8, 10 }, "Dark and emotional. The go-to sound for trap, R&B and most hip hop." },
            { "Harmonic Minor",   { 0, 2, 3, 5, 7, 8, 11 }, "Minor with a raised 7th. That dramatic, exotic pull back to the root." },
            { "Dorian",           { 0, 2, 3, 5, 7, 9, 10 }, "Minor with a brighter 6th. Smooth and soulful, all over neo-soul." },
            { "Phrygian",         { 0, 1, 3, 5, 7, 8, 10 }, "Minor with a flat 2nd. Dark and tense, used a lot in drill." },
            { "Mixolydian",       { 0, 2, 4, 5, 7, 9, 10 }, "Major with a flat 7th. Bluesy and laid back." },
            { "Minor Pentatonic", { 0, 3, 5, 7, 10 },       "Five notes that almost never sound wrong. Great for hooks and melodies." },
            { "Major Pentatonic", { 0, 2, 4, 7, 9 },        "Five happy notes with no clashing half steps." },
            { "Blues",            { 0, 3, 5, 6, 7, 10 },    "Minor pentatonic plus the blue note (the flat 5) for grit." }
        };

        return list;
    }

    inline bool inScale (int note, int root, const Scale& s)
    {
        const int pc = pitchClassFrom (note, root);
        return std::find (s.steps.begin(), s.steps.end(), pc) != s.steps.end();
    }

    //==============================================================================
    // Order matters: major, minor, dim, aug must stay first (diatonic chords use these indexes)
    struct ChordType
    {
        juce::String name, suffix;
        std::vector<int> intervals;
        juce::String about;
    };

    inline const std::vector<ChordType>& chordTypes()
    {
        static const std::vector<ChordType> list {
            { "Major",      "",     { 0, 4, 7 },          "Root, major 3rd, perfect 5th. Stable and bright." },
            { "Minor",      "m",    { 0, 3, 7 },          "Root, minor 3rd, perfect 5th. Dropping the 3rd by one half step is what makes it sound sad." },
            { "Diminished", "dim",  { 0, 3, 6 },          "Two minor 3rds stacked. Tense, it wants to resolve somewhere." },
            { "Augmented",  "aug",  { 0, 4, 8 },          "Two major 3rds stacked. Dreamy and unstable." },
            { "Sus2",       "sus2", { 0, 2, 7 },          "The 3rd swapped for a 2nd. Open, neither happy nor sad." },
            { "Sus4",       "sus4", { 0, 5, 7 },          "The 3rd swapped for a 4th. Sounds like it's waiting to resolve." },
            { "Major 7",    "maj7", { 0, 4, 7, 11 },      "Major plus a major 7th. Smooth, jazzy, very R&B." },
            { "Minor 7",    "m7",   { 0, 3, 7, 10 },      "Minor plus a flat 7th. Warm and moody, huge in R&B and trap." },
            { "Dominant 7", "7",    { 0, 4, 7, 10 },      "Major plus a flat 7th. Bluesy, pulls toward the chord a 5th below." },
            { "Minor 9",    "m9",   { 0, 3, 7, 10, 14 },  "Minor 7 plus the 9th. Lush neo-soul sound." },
            { "Add 9",      "add9", { 0, 4, 7, 14 },      "Major with the 9th added. Bright and modern." }
        };

        return list;
    }

    /** Which chord tone a note is (0 = root), or -1 if it isn't in the chord. */
    inline int chordToneIndex (int note, int chordRoot, const ChordType& c)
    {
        const int pc = pitchClassFrom (note, chordRoot);

        for (size_t i = 0; i < c.intervals.size(); ++i)
            if (wrap12 (c.intervals[i]) == pc)
                return (int) i;

        return -1;
    }

    inline juce::String chordName (int root, const ChordType& c)   { return pitchName (root) + c.suffix; }

    /** Short badge for a chord tone: R, 3, 5, 7, 9... */
    inline juce::String toneShort (int interval)
    {
        switch (interval)
        {
            case 0:          return "R";
            case 2:          return "2";
            case 3: case 4:  return "3";
            case 5:          return "4";
            case 6:          return "b5";
            case 7:          return "5";
            case 8:          return "#5";
            case 10: case 11: return "7";
            case 14:         return "9";
            default:         return {};
        }
    }

    inline juce::String toneLong (int interval)
    {
        switch (interval)
        {
            case 0:          return "Root";
            case 2:          return "2nd";
            case 3:          return "minor 3rd";
            case 4:          return "major 3rd";
            case 5:          return "4th";
            case 6:          return "flat 5th";
            case 7:          return "5th";
            case 8:          return "sharp 5th";
            case 10:         return "flat 7th";
            case 11:         return "major 7th";
            case 14:         return "9th";
            default:         return {};
        }
    }

    //==============================================================================
    struct DiatonicChord
    {
        juce::String numeral;
        int root = 0;
        int type = 0;
    };

    /** The chords that belong to a 7-note key, built by stacking every other scale note. */
    inline std::vector<DiatonicChord> diatonicChords (int keyRoot, const Scale& s)
    {
        std::vector<DiatonicChord> result;

        if (s.steps.size() != 7)
            return result;

        static const juce::StringArray numerals { "I", "II", "III", "IV", "V", "VI", "VII" };

        for (int d = 0; d < 7; ++d)
        {
            const int r = s.steps[(size_t) d];

            auto toneAbove = [&] (int k)
            {
                const int i = d + k;
                return s.steps[(size_t) (i % 7)] + (i >= 7 ? 12 : 0) - r;
            };

            const int third = toneAbove (2);
            const int fifth = toneAbove (4);

            int type = 0;
            juce::String numeral = numerals[d];

            if (third == 3 && fifth == 7)       { type = 1; numeral = numeral.toLowerCase(); }
            else if (third == 3 && fifth == 6)  { type = 2; numeral = numeral.toLowerCase() + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")); }
            else if (third == 4 && fifth == 8)  { type = 3; numeral = numeral + "+"; }

            result.push_back ({ numeral, wrap12 (keyRoot + r), type });
        }

        return result;
    }
}
