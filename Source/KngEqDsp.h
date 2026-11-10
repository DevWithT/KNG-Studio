#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

/** KNG EQ: parameter layout and filter math, shared by the plugin and its graph. */
namespace KngEq
{
    constexpr int numBands = 8;
    constexpr int paramsPerBand = 6;

    enum BandParam { bandOn, bandType, bandFreq, bandGain, bandQ, bandSlope };
    enum Type { lowCut, lowShelf, bell, notch, highShelf, highCut };

    constexpr int outputParam = numBands * paramsPerBand;
    constexpr int numParams = outputParam + 1;

    inline int index (int band, int p)   { return band * paramsPerBand + p; }

    struct ParamDef
    {
        juce::String id, name;
        float min, max, def, mid, step;
    };

    inline const std::vector<ParamDef>& params()
    {
        static const std::vector<ParamDef> list = []
        {
            std::vector<ParamDef> v;
            const float freqs[numBands] = { 30.0f, 100.0f, 250.0f, 800.0f, 2500.0f, 6000.0f, 10000.0f, 18000.0f };
            const int types[numBands]   = { lowCut, lowShelf, bell, bell, bell, bell, highShelf, highCut };

            for (int b = 0; b < numBands; ++b)
            {
                const juce::String n (b + 1);
                const float defQ = types[b] == bell ? 1.0f : 0.707f;

                v.push_back ({ "b" + n + "On",    "Band " + n + " On",     0.0f,  1.0f,     0.0f,             0.0f,    1.0f });
                v.push_back ({ "b" + n + "Type",  "Band " + n + " Type",   0.0f,  5.0f,     (float) types[b], 0.0f,    1.0f });
                v.push_back ({ "b" + n + "Freq",  "Band " + n + " Freq",  20.0f, 20000.0f,  freqs[b],         1000.0f, 0.0f });
                v.push_back ({ "b" + n + "Gain",  "Band " + n + " Gain", -24.0f, 24.0f,     0.0f,             0.0f,    0.0f });
                v.push_back ({ "b" + n + "Q",     "Band " + n + " Q",     0.1f,  18.0f,     defQ,             1.0f,    0.0f });
                v.push_back ({ "b" + n + "Slope", "Band " + n + " Slope", 0.0f,  2.0f,      0.0f,             0.0f,    1.0f });
            }

            v.push_back ({ "output", "Output", -24.0f, 24.0f, 0.0f, 0.0f, 0.0f });
            return v;
        }();

        return list;
    }

    inline const juce::StringArray& typeNames()
    {
        static const juce::StringArray names { "Low Cut", "Low Shelf", "Bell", "Notch", "High Shelf", "High Cut" };
        return names;
    }

    inline const juce::StringArray& slopeNames()
    {
        static const juce::StringArray names { "12 dB/oct", "24 dB/oct", "48 dB/oct" };
        return names;
    }

    //==============================================================================
    struct Coefs
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    // Doubles keep low bands accurate down in the sub range
    struct State
    {
        double z1 = 0.0, z2 = 0.0;

        float process (float x, const Coefs& c)
        {
            const double y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return (float) y;
        }

        void reset()   { z1 = z2 = 0.0; }
    };

    inline Coefs makeBiquad (int type, double f, double gainDb, double q, double sr)
    {
        f = juce::jlimit (10.0, sr * 0.49, f);

        const double w0 = juce::MathConstants<double>::twoPi * f / sr;
        const double cw = std::cos (w0);
        const double sw = std::sin (w0);
        const double alpha = sw / (2.0 * juce::jmax (0.05, q));
        const double A = std::pow (10.0, gainDb / 40.0);
        const double sq = 2.0 * std::sqrt (A) * alpha;

        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a0 = 1.0, a1 = 0.0, a2 = 0.0;

        switch (type)
        {
            case lowCut:   // high pass
                b0 = (1.0 + cw) * 0.5; b1 = -(1.0 + cw); b2 = (1.0 + cw) * 0.5;
                a0 = 1.0 + alpha;      a1 = -2.0 * cw;   a2 = 1.0 - alpha;
                break;

            case highCut:  // low pass
                b0 = (1.0 - cw) * 0.5; b1 = 1.0 - cw;    b2 = (1.0 - cw) * 0.5;
                a0 = 1.0 + alpha;      a1 = -2.0 * cw;   a2 = 1.0 - alpha;
                break;

            case bell:
                b0 = 1.0 + alpha * A;  b1 = -2.0 * cw;   b2 = 1.0 - alpha * A;
                a0 = 1.0 + alpha / A;  a1 = -2.0 * cw;   a2 = 1.0 - alpha / A;
                break;

            case notch:
                b0 = 1.0;              b1 = -2.0 * cw;   b2 = 1.0;
                a0 = 1.0 + alpha;      a1 = -2.0 * cw;   a2 = 1.0 - alpha;
                break;

            case lowShelf:
                b0 = A * ((A + 1.0) - (A - 1.0) * cw + sq);
                b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
                b2 = A * ((A + 1.0) - (A - 1.0) * cw - sq);
                a0 = (A + 1.0) + (A - 1.0) * cw + sq;
                a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
                a2 = (A + 1.0) + (A - 1.0) * cw - sq;
                break;

            case highShelf:
                b0 = A * ((A + 1.0) + (A - 1.0) * cw + sq);
                b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
                b2 = A * ((A + 1.0) + (A - 1.0) * cw - sq);
                a0 = (A + 1.0) - (A - 1.0) * cw + sq;
                a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
                a2 = (A + 1.0) - (A - 1.0) * cw - sq;
                break;

            default:
                break;
        }

        Coefs c;
        c.b0 = b0 / a0;
        c.b1 = b1 / a0;
        c.b2 = b2 / a0;
        c.a1 = a1 / a0;
        c.a2 = a2 / a0;
        return c;
    }

    /** Builds one band. Cuts stack 1, 2 or 4 stages for 12, 24 or 48 dB/oct. Returns the stage count. */
    inline int makeBand (int type, int slope, double f, double gainDb, double q, double sr, std::array<Coefs, 4>& out)
    {
        if (type == lowCut || type == highCut)
        {
            // Butterworth stage Qs, so steep cuts stay flat right up to the corner
            static const double q24[] = { 0.5412, 1.3066 };
            static const double q48[] = { 0.5098, 0.6013, 0.9000, 2.5629 };

            const int stages = slope == 0 ? 1 : slope == 1 ? 2 : 4;

            for (int s = 0; s < stages; ++s)
            {
                const double stageQ = slope == 0 ? q : slope == 1 ? q24[s] : q48[s];
                out[(size_t) s] = makeBiquad (type, f, 0.0, stageQ, sr);
            }

            return stages;
        }

        out[0] = makeBiquad (type, f, gainDb, q, sr);
        return 1;
    }

    /** How much one filter stage boosts or cuts at a frequency, for drawing the curve. */
    inline double magnitudeDb (const Coefs& c, double f, double sr)
    {
        const double w = juce::MathConstants<double>::twoPi * f / sr;
        const std::complex<double> z1 = std::polar (1.0, -w);
        const std::complex<double> z2 = z1 * z1;
        const auto h = (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
        return 20.0 * std::log10 (juce::jmax (1.0e-9, std::abs (h)));
    }
}
