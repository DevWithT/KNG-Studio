#pragma once

#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

/** KNG Verb: parameters and presets. */
namespace KngVerb
{
    enum Param { preDelay, size, decay, damping, diffusion, modulation, lowCut, highCut, width, duck, mix, numParams };

    struct ParamInfo
    {
        const char* id;
        const char* name;
        float min, max, def, mid, step;
    };

    inline const ParamInfo& info (int p)
    {
        static const ParamInfo table[numParams] = {
            { "preDelay",   "Pre-Delay",     0.0f,   250.0f,   20.0f,   40.0f, 0.0f },
            { "size",       "Size",          0.0f,     1.0f,    0.5f,    0.0f, 0.0f },
            { "decay",      "Decay",         0.1f,    20.0f,    2.0f,    2.0f, 0.0f },
            { "damping",    "Damping",    1000.0f, 20000.0f, 7000.0f, 6000.0f, 0.0f },
            { "diffusion",  "Diffusion",     0.0f,     1.0f,    0.7f,    0.0f, 0.0f },
            { "modulation", "Modulation",    0.0f,     1.0f,    0.3f,    0.0f, 0.0f },
            { "lowCut",     "Low Cut",      20.0f,  1000.0f,  150.0f,  150.0f, 0.0f },
            { "highCut",    "High Cut",   1000.0f, 20000.0f, 9000.0f, 6000.0f, 0.0f },
            { "width",      "Width",         0.0f,     1.0f,    1.0f,    0.0f, 0.0f },
            { "duck",       "Duck",          0.0f,     1.0f,    0.0f,    0.0f, 0.0f },
            { "mix",        "Mix",           0.0f,     1.0f,    0.3f,    0.0f, 0.0f }
        };

        return table[p];
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

            { "Vocal Plate", { { size, 0.45f }, { decay, 1.6f }, { preDelay, 30.0f }, { damping, 9000.0f },
                               { diffusion, 0.85f }, { modulation, 0.35f }, { lowCut, 200.0f }, { highCut, 10000.0f },
                               { mix, 0.25f } } },

            { "Trap Vocal Wash", { { size, 0.8f }, { decay, 3.5f }, { preDelay, 40.0f }, { damping, 7000.0f },
                                   { diffusion, 0.8f }, { modulation, 0.5f }, { lowCut, 250.0f }, { highCut, 8000.0f },
                                   { duck, 0.7f }, { mix, 0.3f } } },

            { "Small Room", { { size, 0.2f }, { decay, 0.6f }, { preDelay, 5.0f }, { damping, 8000.0f },
                              { diffusion, 0.6f }, { modulation, 0.15f }, { lowCut, 120.0f }, { highCut, 12000.0f },
                              { mix, 0.2f } } },

            { "Big Hall", { { size, 0.95f }, { decay, 4.5f }, { preDelay, 35.0f }, { damping, 6000.0f },
                            { diffusion, 0.9f }, { modulation, 0.4f }, { lowCut, 150.0f }, { highCut, 9000.0f },
                            { mix, 0.3f } } },

            { "Snare Room", { { size, 0.35f }, { decay, 1.0f }, { preDelay, 10.0f }, { damping, 10000.0f },
                              { diffusion, 0.7f }, { lowCut, 200.0f }, { highCut, 12000.0f }, { mix, 0.3f } } },

            { "Dark Plate", { { size, 0.5f }, { decay, 2.2f }, { preDelay, 20.0f }, { damping, 3500.0f },
                              { diffusion, 0.9f }, { highCut, 5000.0f }, { mix, 0.25f } } },

            { "Ambient Space", { { size, 1.0f }, { decay, 12.0f }, { preDelay, 60.0f }, { damping, 5000.0f },
                                 { diffusion, 1.0f }, { modulation, 0.8f }, { lowCut, 200.0f }, { highCut, 7000.0f },
                                 { mix, 0.4f } } },

            { "Send Bus", { { mix, 1.0f } } }
        };

        return list;
    }
}
