#pragma once

#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

/** KNG Comp: parameters, the gain curve, and presets. */
namespace KngComp
{
    enum Param { threshold, ratio, attack, release, knee, makeup, autoMakeup, mix, mode, scHpf, numParams };
    enum Mode { clean, punch, opto, vintage };

    struct ParamInfo
    {
        const char* id;
        const char* name;
        float min, max, def, mid, step;
    };

    inline const ParamInfo& info (int p)
    {
        static const ParamInfo table[numParams] = {
            { "threshold",  "Threshold", -60.0f,    0.0f, -18.0f,   0.0f, 0.0f },
            { "ratio",      "Ratio",       1.0f,   20.0f,   4.0f,   4.0f, 0.0f },
            { "attack",     "Attack",      0.1f,  100.0f,  10.0f,  10.0f, 0.0f },
            { "release",    "Release",    10.0f, 2000.0f, 120.0f, 150.0f, 0.0f },
            { "knee",       "Knee",        0.0f,   18.0f,   6.0f,   0.0f, 0.0f },
            { "makeup",     "Makeup",    -12.0f,   24.0f,   0.0f,   0.0f, 0.0f },
            { "autoMakeup", "Auto Gain",   0.0f,    1.0f,   1.0f,   0.0f, 1.0f },
            { "mix",        "Mix",         0.0f,    1.0f,   1.0f,   0.0f, 0.0f },
            { "mode",       "Mode",        0.0f,    3.0f,   0.0f,   0.0f, 1.0f },
            { "scHpf",      "SC Filter",  20.0f,  500.0f,  20.0f, 100.0f, 0.0f }
        };

        return table[p];
    }

    inline const juce::StringArray& modeNames()
    {
        static const juce::StringArray names { "Clean", "Punch", "Opto", "Vintage" };
        return names;
    }

    /** How many dB of reduction at a given input level (soft knee). Always 0 or negative. */
    inline float gainComputer (float inputDb, float thresholdDb, float ratioValue, float kneeDb)
    {
        const float over = inputDb - thresholdDb;
        const float slope = 1.0f / juce::jmax (1.0f, ratioValue) - 1.0f;

        if (2.0f * over < -kneeDb)
            return 0.0f;

        if (kneeDb > 0.0f && 2.0f * std::abs (over) <= kneeDb)
        {
            const float t = over + kneeDb * 0.5f;
            return slope * t * t / (2.0f * kneeDb);
        }

        return slope * over;
    }

    //==============================================================================
    struct Preset
    {
        const char* name;
        std::vector<std::pair<int, float>> values;
    };

    inline const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> list {
            { "Default", {} },

            { "Vocal Smooth", { { mode, 2.0f }, { threshold, -20.0f }, { ratio, 3.0f }, { attack, 15.0f },
                                { release, 200.0f }, { knee, 9.0f } } },

            { "Vocal Aggressive", { { mode, 3.0f }, { threshold, -24.0f }, { ratio, 6.0f }, { attack, 2.0f },
                                    { release, 80.0f }, { knee, 3.0f } } },

            { "Drum Bus Glue", { { mode, 0.0f }, { threshold, -16.0f }, { ratio, 2.0f }, { attack, 30.0f },
                                 { release, 150.0f }, { knee, 6.0f }, { scHpf, 90.0f } } },

            { "Kick Punch", { { mode, 1.0f }, { threshold, -14.0f }, { ratio, 4.0f }, { attack, 20.0f },
                              { release, 60.0f }, { knee, 3.0f } } },

            { "808 Control", { { mode, 0.0f }, { threshold, -12.0f }, { ratio, 4.0f }, { attack, 8.0f },
                               { release, 120.0f }, { knee, 6.0f } } },

            { "Master Glue", { { mode, 0.0f }, { threshold, -10.0f }, { ratio, 1.5f }, { attack, 30.0f },
                               { release, 300.0f }, { knee, 9.0f }, { scHpf, 60.0f } } },

            { "Parallel Smash", { { mode, 3.0f }, { threshold, -35.0f }, { ratio, 10.0f }, { attack, 1.0f },
                                  { release, 50.0f }, { mix, 0.35f } } }
        };

        return list;
    }
}
