#pragma once

#include "KngSynthParams.h"
#include <utility>
#include <vector>

/** Factory sounds. Each preset starts from the defaults and changes only what it lists. */
namespace KngSynth
{
    struct Preset
    {
        const char* name;
        std::vector<std::pair<int, float>> values;
    };

    inline const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> list {
            { "Init", {} },

            { "808 Sub", { { oscAWave, 3 }, { oscAOct, -1 }, { subLevel, 0.4f }, { drive, 0.35f }, { cutoff, 6000 },
                           { ampA, 0.002f }, { ampD, 2.0f }, { ampS, 0.6f }, { ampR, 0.25f },
                           { voiceMode, 2 }, { glide, 0.08f }, { velSens, 0.3f } } },

            { "Trap Lead", { { oscAUnison, 3 }, { oscADetune, 0.3f }, { oscBWave, 1 }, { oscBOct, 1 }, { oscBLevel, 0.4f },
                             { cutoff, 3500 }, { reso, 0.2f }, { ampA, 0.005f }, { ampD, 0.3f }, { ampS, 0.7f }, { ampR, 0.25f },
                             { lfoRate, 5.5f }, { lfoPitch, 0.04f }, { voiceMode, 1 }, { glide, 0.04f } } },

            { "Supersaw", { { oscAUnison, 7 }, { oscADetune, 0.6f }, { oscBUnison, 7 }, { oscBDetune, 0.5f }, { oscBOct, 1 },
                            { oscBLevel, 0.5f }, { spread, 1.0f }, { cutoff, 9000 }, { ampR, 0.6f } } },

            { "Pluck", { { oscBWave, 1 }, { oscBLevel, 0.5f }, { cutoff, 600 }, { filterEnv, 0.7f },
                         { fenvD, 0.25f }, { fenvS, 0.0f }, { ampD, 0.4f }, { ampS, 0.0f }, { ampR, 0.2f } } },

            { "Soft Pad", { { oscAWave, 2 }, { oscAUnison, 3 }, { oscADetune, 0.25f }, { oscBLevel, 0.3f }, { oscBUnison, 5 },
                            { oscBDetune, 0.4f }, { spread, 0.9f }, { cutoff, 1800 }, { ampA, 0.8f }, { ampD, 1.0f },
                            { ampS, 0.8f }, { ampR, 2.0f }, { lfoRate, 0.3f }, { lfoCutoff, 0.2f } } },

            { "Reese Bass", { { oscAUnison, 2 }, { oscADetune, 0.35f }, { oscBFine, 12 }, { oscBLevel, 0.8f },
                              { cutoff, 700 }, { reso, 0.3f }, { drive, 0.4f }, { voiceMode, 1 }, { spread, 0.3f } } },

            { "Bell", { { oscAWave, 3 }, { oscBWave, 2 }, { oscBOct, 1 }, { oscBSemi, 7 }, { oscBLevel, 0.4f },
                        { ampA, 0.001f }, { ampD, 1.5f }, { ampS, 0.0f }, { ampR, 1.5f } } },

            { "Dark Keys", { { oscAWave, 1 }, { oscALevel, 0.6f }, { oscBFine, -8 }, { oscBLevel, 0.5f }, { cutoff, 1200 },
                             { filterEnv, 0.3f }, { fenvD, 0.6f }, { fenvS, 0.3f }, { ampA, 0.005f }, { ampD, 0.8f },
                             { ampS, 0.5f }, { ampR, 0.4f } } }
        };

        return list;
    }
}
