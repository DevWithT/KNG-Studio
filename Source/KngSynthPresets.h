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
                             { ampS, 0.5f }, { ampR, 0.4f } } },

            // Leads
            { "Lead - Hypnotic Saw", { { oscAUnison, 5 }, { oscADetune, 0.35f }, { cutoff, 4500 }, { reso, 0.15f },
                                       { ampA, 0.005f }, { ampS, 0.8f }, { ampR, 0.3f }, { lfoRate, 6.0f }, { lfoPitch, 0.03f },
                                       { voiceMode, 1 }, { glide, 0.06f } } },

            { "Lead - Square Whistle", { { oscAWave, 1 }, { oscBWave, 3 }, { oscBOct, 1 }, { oscBLevel, 0.3f }, { cutoff, 5000 },
                                         { ampS, 0.9f }, { ampR, 0.2f }, { lfoRate, 5.0f }, { lfoPitch, 0.05f },
                                         { voiceMode, 2 }, { glide, 0.1f } } },

            { "Lead - Siren", { { oscAWave, 2 }, { oscBSemi, 7 }, { oscBLevel, 0.35f }, { cutoff, 3000 }, { reso, 0.35f },
                                { lfoRate, 4.0f }, { lfoPitch, 0.15f }, { voiceMode, 1 }, { glide, 0.12f } } },

            // Plucks
            { "Pluck - Glass", { { oscAWave, 3 }, { oscBWave, 2 }, { oscBOct, 2 }, { oscBLevel, 0.35f }, { cutoff, 2500 },
                                 { filterEnv, 0.5f }, { fenvD, 0.15f }, { ampD, 0.5f }, { ampS, 0.0f }, { ampR, 0.4f } } },

            { "Pluck - Saw Stab", { { oscAUnison, 3 }, { oscADetune, 0.3f }, { cutoff, 900 }, { reso, 0.25f }, { filterEnv, 0.8f },
                                    { fenvD, 0.18f }, { ampD, 0.25f }, { ampS, 0.0f }, { ampR, 0.15f } } },

            { "Pluck - Marimba", { { oscAWave, 3 }, { oscBWave, 3 }, { oscBOct, 2 }, { oscBLevel, 0.25f }, { cutoff, 4000 },
                                   { ampA, 0.001f }, { ampD, 0.35f }, { ampS, 0.0f }, { ampR, 0.3f } } },

            // Pads
            { "Pad - Night Air", { { oscAUnison, 5 }, { oscADetune, 0.3f }, { oscBWave, 2 }, { oscBOct, 1 }, { oscBLevel, 0.3f },
                                   { noiseLevel, 0.03f }, { spread, 1.0f }, { cutoff, 1400 }, { reso, 0.1f },
                                   { ampA, 1.5f }, { ampD, 1.0f }, { ampS, 0.9f }, { ampR, 3.0f }, { lfoRate, 0.2f }, { lfoCutoff, 0.25f } } },

            { "Pad - Warm Strings", { { oscAUnison, 7 }, { oscADetune, 0.2f }, { spread, 0.8f }, { cutoff, 2500 },
                                      { ampA, 0.6f }, { ampS, 0.9f }, { ampR, 1.5f }, { lfoRate, 5.0f }, { lfoPitch, 0.01f } } },

            { "Pad - Dark Choir", { { oscAWave, 2 }, { oscBWave, 2 }, { oscBFine, 10 }, { oscBLevel, 0.6f }, { filterType, 2 },
                                    { cutoff, 900 }, { reso, 0.3f }, { ampA, 1.0f }, { ampS, 0.9f }, { ampR, 2.5f } } },

            // Bells and keys
            { "Bell - Music Box", { { oscAWave, 3 }, { oscAOct, 1 }, { oscBWave, 3 }, { oscBOct, 2 }, { oscBSemi, 7 },
                                    { oscBLevel, 0.3f }, { ampD, 1.2f }, { ampS, 0.0f }, { ampR, 1.2f } } },

            { "Bell - Horror", { { oscAWave, 3 }, { oscBSemi, 6 }, { oscBLevel, 0.25f }, { cutoff, 3000 },
                                 { ampD, 2.0f }, { ampS, 0.0f }, { ampR, 2.0f }, { lfoRate, 3.0f }, { lfoPitch, 0.03f } } },

            { "Keys - Rhodes-ish", { { oscAWave, 3 }, { oscBWave, 2 }, { oscBOct, 1 }, { oscBLevel, 0.25f }, { cutoff, 2200 },
                                     { filterEnv, 0.25f }, { fenvD, 0.5f }, { ampD, 1.2f }, { ampS, 0.3f }, { ampR, 0.5f },
                                     { lfoRate, 4.5f }, { lfoCutoff, 0.05f } } },

            // Bass
            { "Bass - Wobble", { { oscAOct, -1 }, { oscAUnison, 3 }, { oscADetune, 0.3f }, { cutoff, 500 }, { reso, 0.45f },
                                 { drive, 0.5f }, { lfoRate, 4.0f }, { lfoCutoff, 0.7f }, { voiceMode, 1 } } },

            { "Bass - Sub Sine", { { oscAWave, 3 }, { oscAOct, -1 }, { subLevel, 0.3f }, { cutoff, 2000 },
                                   { ampR, 0.15f }, { voiceMode, 1 }, { glide, 0.05f } } },

            { "Bass - Growl", { { oscBWave, 1 }, { oscBFine, -15 }, { oscBLevel, 0.8f }, { cutoff, 800 }, { reso, 0.4f },
                                { drive, 0.7f }, { filterEnv, 0.4f }, { fenvD, 0.3f }, { voiceMode, 1 } } },

            // FX
            { "FX - Noise Riser", { { oscALevel, 0.0f }, { noiseLevel, 0.8f }, { filterType, 1 }, { cutoff, 300 },
                                    { filterEnv, 0.9f }, { fenvA, 3.0f }, { fenvS, 1.0f }, { ampA, 2.0f }, { ampS, 1.0f }, { ampR, 1.0f } } }
        };

        return list;
    }
}
