#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <utility>
#include <vector>

/** KNG Saturator: parameters, the four shapes, and presets. */
namespace KngSat
{
    enum Param { mode, drive, tone, mix, output, autoGain, numParams };
    enum Mode { tape, tube, softClip, hardClip };

    struct ParamInfo
    {
        const char* id;
        const char* name;
        float min, max, def, mid, step;
    };

    inline const ParamInfo& info (int p)
    {
        static const ParamInfo table[numParams] = {
            { "mode",     "Mode",       0.0f,  3.0f, 0.0f, 0.0f, 1.0f },
            { "drive",    "Drive",      0.0f, 24.0f, 6.0f, 0.0f, 0.0f },
            { "tone",     "Tone",      -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
            { "mix",      "Mix",        0.0f,  1.0f, 1.0f, 0.0f, 0.0f },
            { "output",   "Output",   -24.0f, 12.0f, 0.0f, 0.0f, 0.0f },
            { "autoGain", "Auto Gain",  0.0f,  1.0f, 1.0f, 0.0f, 1.0f }
        };

        return table[p];
    }

    inline const juce::StringArray& modeNames()
    {
        static const juce::StringArray names { "Tape", "Tube", "Soft Clip", "Hard Clip" };
        return names;
    }

    /** The shape for each flavor. Input is already driven. */
    inline float shape (int m, float x)
    {
        switch (m)
        {
            case tape:
                return std::tanh (x);

            case tube:
                // Uneven: the two halves bend differently, which adds body (even harmonics)
                return x >= 0.0f ? std::tanh (x) : std::tanh (x * 0.6f) / 0.6f * 0.8f;

            case softClip:
            {
                const float c = juce::jlimit (-1.5f, 1.5f, x);
                return (c - (c * c * c) / 6.75f) * (1.0f / 1.0f);
            }

            default:
                return juce::jlimit (-1.0f, 1.0f, x);
        }
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
            { "808 Grit",          { { mode, 1.0f }, { drive, 10.0f }, { tone, 0.2f },  { mix, 0.6f } } },
            { "808 Phone Speaker", { { mode, 0.0f }, { drive, 14.0f }, { tone, 0.4f },  { mix, 0.7f } } },
            { "Vocal Warmth",      { { mode, 0.0f }, { drive, 4.0f },  { tone, 0.1f },  { mix, 0.5f } } },
            { "Vocal Edge",        { { mode, 1.0f }, { drive, 8.0f },  { tone, 0.3f },  { mix, 0.4f } } },
            { "Drum Crunch",       { { mode, 2.0f }, { drive, 12.0f }, { mix, 0.5f } } },
            { "Hard Clipper",      { { mode, 3.0f }, { drive, 6.0f },  { mix, 1.0f } } },
            { "Lo-Fi Fuzz",        { { mode, 3.0f }, { drive, 20.0f }, { tone, -0.5f }, { mix, 0.7f } } }
        };

        return list;
    }
}
