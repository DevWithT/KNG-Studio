#pragma once

#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

/** KNG Delay: parameters, note lengths and presets. */
namespace KngDelay
{
    enum Param { mode, sync, divL, divR, timeL, timeR, feedback, lowCut, highCut, drive, wobble, width, duck, mix, numParams };
    enum Mode { stereo, pingPong, mono };

    struct ParamInfo
    {
        const char* id;
        const char* name;
        float min, max, def, mid, step;
    };

    inline const ParamInfo& info (int p)
    {
        static const ParamInfo table[numParams] = {
            { "mode",     "Mode",        0.0f,     2.0f,    1.0f,    0.0f, 1.0f },
            { "sync",     "Sync",        0.0f,     1.0f,    1.0f,    0.0f, 1.0f },
            { "divL",     "Left",        0.0f,    11.0f,    5.0f,    0.0f, 1.0f },
            { "divR",     "Right",       0.0f,    11.0f,    5.0f,    0.0f, 1.0f },
            { "timeL",    "Left",        1.0f,  2000.0f,  250.0f,  300.0f, 0.0f },
            { "timeR",    "Right",       1.0f,  2000.0f,  375.0f,  300.0f, 0.0f },
            { "feedback", "Feedback",    0.0f,     0.95f,   0.35f,   0.0f, 0.0f },
            { "lowCut",   "Low Cut",    20.0f,  2000.0f,  200.0f,  300.0f, 0.0f },
            { "highCut",  "High Cut", 1000.0f, 20000.0f, 6000.0f, 6000.0f, 0.0f },
            { "drive",    "Drive",       0.0f,     1.0f,    0.2f,    0.0f, 0.0f },
            { "wobble",   "Wobble",      0.0f,     1.0f,    0.1f,    0.0f, 0.0f },
            { "width",    "Width",       0.0f,     1.0f,    1.0f,    0.0f, 0.0f },
            { "duck",     "Duck",        0.0f,     1.0f,    0.0f,    0.0f, 0.0f },
            { "mix",      "Mix",         0.0f,     1.0f,    0.25f,   0.0f, 0.0f }
        };

        return table[p];
    }

    inline const juce::StringArray& modeNames()
    {
        static const juce::StringArray names { "Stereo", "Ping-Pong", "Mono" };
        return names;
    }

    inline const juce::StringArray& divisionNames()
    {
        static const juce::StringArray names { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
                                               "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1 Bar" };
        return names;
    }

    inline double divisionBeats (int i)
    {
        static const double beats[12] = { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5,
                                          0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0 };
        return beats[juce::jlimit (0, 11, i)];
    }

    /** Delay time in seconds for the left (0) or right (1) side. */
    inline double delaySeconds (const float* v, int side, double bpm)
    {
        if (v[sync] > 0.5f)
            return divisionBeats (juce::roundToInt (v[side == 0 ? divL : divR])) * 60.0 / juce::jmax (20.0, bpm);

        return v[side == 0 ? timeL : timeR] * 0.001;
    }

    struct Preset
    {
        const char* name;
        std::vector<std::pair<int, float>> values;
    };

    inline const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> list {
            { "Default", {} },

            { "Vocal Eighth", { { mode, 1.0f }, { divL, 5.0f }, { divR, 5.0f }, { feedback, 0.3f },
                                { lowCut, 300.0f }, { highCut, 5000.0f }, { duck, 0.5f }, { mix, 0.2f } } },

            { "Trap Ad-lib Throw", { { mode, 1.0f }, { divL, 8.0f }, { divR, 8.0f }, { feedback, 0.45f },
                                     { drive, 0.3f }, { duck, 0.6f }, { mix, 0.3f } } },

            { "Slapback", { { mode, 2.0f }, { sync, 0.0f }, { timeL, 110.0f }, { feedback, 0.1f },
                            { highCut, 7000.0f }, { mix, 0.25f } } },

            { "Wide Stereo", { { mode, 0.0f }, { divL, 5.0f }, { divR, 6.0f }, { feedback, 0.35f }, { mix, 0.2f } } },

            { "Tape Echo", { { mode, 0.0f }, { divL, 9.0f }, { divR, 9.0f }, { feedback, 0.5f }, { drive, 0.6f },
                             { wobble, 0.5f }, { highCut, 3500.0f }, { mix, 0.25f } } },

            { "Dub Space", { { mode, 1.0f }, { divL, 9.0f }, { divR, 9.0f }, { feedback, 0.7f }, { lowCut, 400.0f },
                             { highCut, 2500.0f }, { drive, 0.4f }, { mix, 0.35f } } },

            { "Send Bus", { { mix, 1.0f } } }
        };

        return list;
    }
}
