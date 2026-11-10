#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/** Every KNG Synth knob: id, name, range, default, and how it's shown. */
namespace KngSynth
{
    enum Param
    {
        oscAWave, oscAOct, oscASemi, oscAFine, oscALevel, oscAUnison, oscADetune,
        oscBWave, oscBOct, oscBSemi, oscBFine, oscBLevel, oscBUnison, oscBDetune,
        subLevel, noiseLevel, spread,
        filterType, cutoff, reso, drive, filterEnv, keyTrack,
        fenvA, fenvD, fenvS, fenvR,
        ampA, ampD, ampS, ampR,
        lfoShape, lfoRate, lfoPitch, lfoCutoff,
        voiceMode, glide, velSens, master,
        numParams
    };

    /** mid = the value at the knob's centre (0 for an even knob). step = 1 for whole numbers.
        choices = "|"-separated names for a drop-down instead of a knob. */
    struct ParamInfo
    {
        const char* id;
        const char* name;
        float min, max, def, mid, step;
        const char* choices;
    };

    inline const ParamInfo& info (int p)
    {
        static const ParamInfo table[numParams] = {
            { "oscAWave",   "Wave",     0.0f,    3.0f,     0.0f,    0.0f,  1.0f, "Saw|Square|Triangle|Sine" },
            { "oscAOct",    "Octave",  -3.0f,    3.0f,     0.0f,    0.0f,  1.0f, nullptr },
            { "oscASemi",   "Semi",   -12.0f,   12.0f,     0.0f,    0.0f,  1.0f, nullptr },
            { "oscAFine",   "Fine",  -100.0f,  100.0f,     0.0f,    0.0f,  0.0f, nullptr },
            { "oscALevel",  "Level",    0.0f,    1.0f,     0.8f,    0.0f,  0.0f, nullptr },
            { "oscAUnison", "Unison",   1.0f,    7.0f,     1.0f,    0.0f,  1.0f, nullptr },
            { "oscADetune", "Detune",   0.0f,    1.0f,     0.25f,   0.0f,  0.0f, nullptr },

            { "oscBWave",   "Wave",     0.0f,    3.0f,     0.0f,    0.0f,  1.0f, "Saw|Square|Triangle|Sine" },
            { "oscBOct",    "Octave",  -3.0f,    3.0f,     0.0f,    0.0f,  1.0f, nullptr },
            { "oscBSemi",   "Semi",   -12.0f,   12.0f,     0.0f,    0.0f,  1.0f, nullptr },
            { "oscBFine",   "Fine",  -100.0f,  100.0f,     0.0f,    0.0f,  0.0f, nullptr },
            { "oscBLevel",  "Level",    0.0f,    1.0f,     0.0f,    0.0f,  0.0f, nullptr },
            { "oscBUnison", "Unison",   1.0f,    7.0f,     1.0f,    0.0f,  1.0f, nullptr },
            { "oscBDetune", "Detune",   0.0f,    1.0f,     0.25f,   0.0f,  0.0f, nullptr },

            { "sub",        "Sub",      0.0f,    1.0f,     0.0f,    0.0f,  0.0f, nullptr },
            { "noise",      "Noise",    0.0f,    1.0f,     0.0f,    0.0f,  0.0f, nullptr },
            { "spread",     "Width",    0.0f,    1.0f,     0.5f,    0.0f,  0.0f, nullptr },

            { "filterType", "Type",     0.0f,    2.0f,     0.0f,    0.0f,  1.0f, "Low Pass|High Pass|Band Pass" },
            { "cutoff",     "Cutoff",  20.0f, 20000.0f, 20000.0f, 1000.0f, 0.0f, nullptr },
            { "reso",       "Reso",     0.0f,    1.0f,     0.1f,    0.0f,  0.0f, nullptr },
            { "drive",      "Drive",    0.0f,    1.0f,     0.0f,    0.0f,  0.0f, nullptr },
            { "filterEnv",  "Env Amt", -1.0f,    1.0f,     0.0f,    0.0f,  0.0f, nullptr },
            { "keyTrack",   "Key Trk",  0.0f,    1.0f,     0.0f,    0.0f,  0.0f, nullptr },

            { "fenvA",      "Attack",   0.001f,  5.0f,     0.005f,  0.3f,  0.0f, nullptr },
            { "fenvD",      "Decay",    0.001f,  5.0f,     0.3f,    0.5f,  0.0f, nullptr },
            { "fenvS",      "Sustain",  0.0f,    1.0f,     0.0f,    0.0f,  0.0f, nullptr },
            { "fenvR",      "Release",  0.001f,  8.0f,     0.3f,    0.6f,  0.0f, nullptr },

            { "ampA",       "Attack",   0.001f,  5.0f,     0.005f,  0.3f,  0.0f, nullptr },
            { "ampD",       "Decay",    0.001f,  5.0f,     0.3f,    0.5f,  0.0f, nullptr },
            { "ampS",       "Sustain",  0.0f,    1.0f,     1.0f,    0.0f,  0.0f, nullptr },
            { "ampR",       "Release",  0.001f,  8.0f,     0.25f,   0.6f,  0.0f, nullptr },

            { "lfoShape",   "Shape",    0.0f,    3.0f,     0.0f,    0.0f,  1.0f, "Sine|Triangle|Saw|Square" },
            { "lfoRate",    "Rate",     0.05f,  20.0f,     2.0f,    2.0f,  0.0f, nullptr },
            { "lfoPitch",   "Vibrato",  0.0f,    1.0f,     0.0f,    0.0f,  0.0f, nullptr },
            { "lfoCutoff",  "Wobble",   0.0f,    1.0f,     0.0f,    0.0f,  0.0f, nullptr },

            { "voiceMode",  "Mode",     0.0f,    2.0f,     0.0f,    0.0f,  1.0f, "Poly|Mono|Legato" },
            { "glide",      "Glide",    0.0f,    1.0f,     0.0f,    0.15f, 0.0f, nullptr },
            { "velSens",    "Velocity", 0.0f,    1.0f,     0.6f,    0.0f,  0.0f, nullptr },
            { "master",     "Volume", -24.0f,    6.0f,    -3.0f,    0.0f,  0.0f, nullptr }
        };

        return table[p];
    }

    struct Section
    {
        const char* name;
        std::vector<int> params;
    };

    inline const std::vector<Section>& sections()
    {
        static const std::vector<Section> list {
            { "Osc A",       { oscAWave, oscAOct, oscASemi, oscAFine, oscALevel, oscAUnison, oscADetune } },
            { "Osc B",       { oscBWave, oscBOct, oscBSemi, oscBFine, oscBLevel, oscBUnison, oscBDetune } },
            { "Sub / Noise", { subLevel, noiseLevel, spread } },
            { "Filter",      { filterType, cutoff, reso, drive, filterEnv, keyTrack } },
            { "Filter Env",  { fenvA, fenvD, fenvS, fenvR } },
            { "Amp Env",     { ampA, ampD, ampS, ampR } },
            { "LFO",         { lfoShape, lfoRate, lfoPitch, lfoCutoff } },
            { "Voice",       { voiceMode, glide, velSens, master } }
        };

        return list;
    }
}
