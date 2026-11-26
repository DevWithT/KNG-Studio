#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <vector>

/** Finds the note a short piece of audio is playing, and how far it is from the nearest note of a scale. */
namespace KngSlicePitch
{
    struct Scale
    {
        const char* name;
        std::vector<int> notes;   // semitones above the key note
    };

    inline const std::vector<Scale>& scales()
    {
        static const std::vector<Scale> list = {
            { "Major",            { 0, 2, 4, 5, 7, 9, 11 } },
            { "Minor",            { 0, 2, 3, 5, 7, 8, 10 } },
            { "Harmonic Minor",   { 0, 2, 3, 5, 7, 8, 11 } },
            { "Pentatonic Major", { 0, 2, 4, 7, 9 } },
            { "Pentatonic Minor", { 0, 3, 5, 7, 10 } },
            { "Blues",            { 0, 3, 5, 6, 7, 10 } },
            { "Chromatic",        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } }
        };

        return list;
    }

    /** The pitch in Hz of a mono piece of audio, or 0 if there is no clear note. Uses the YIN method, 60 Hz to 1 kHz. */
    inline float detect (const float* x, int n, double sampleRate)
    {
        const int minLag = juce::jmax (2, (int) (sampleRate / 1000.0));
        const int maxLag = juce::jmin (n / 2, (int) (sampleRate / 60.0));

        if (maxLag <= minLag + 2)
            return 0.0f;

        const int w = n - maxLag;
        double energy = 0.0;

        for (int j = 0; j < w; ++j)
            energy += (double) x[j] * x[j];

        if (energy / w < 1.0e-6)
            return 0.0f;

        std::vector<float> d ((size_t) maxLag + 2, 0.0f), dn ((size_t) maxLag + 2, 1.0f);

        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double sum = 0.0;

            for (int j = 0; j < w; ++j)
            {
                const double diff = (double) x[j] - (double) x[j + tau];
                sum += diff * diff;
            }

            d[(size_t) tau] = (float) sum;
        }

        double running = 0.0;

        for (int tau = 1; tau <= maxLag; ++tau)
        {
            running += d[(size_t) tau];
            dn[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
        }

        int best = -1;

        for (int tau = minLag; tau < maxLag; ++tau)
        {
            if (dn[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < maxLag && dn[(size_t) tau + 1] < dn[(size_t) tau])
                    ++tau;

                best = tau;
                break;
            }
        }

        if (best < 0)
            return 0.0f;

        float t = (float) best;
        const float a = dn[(size_t) best - 1], b = dn[(size_t) best], c = dn[(size_t) best + 1];
        const float denom = a - 2.0f * b + c;

        if (std::abs (denom) > 1.0e-9f)
            t += 0.5f * (a - c) / denom;

        return t > 1.0f ? (float) (sampleRate / t) : 0.0f;
    }

    /** How many semitones to move a note of this pitch so it lands on the nearest note of the scale.
        root is 0 for C up to 11 for B. The answer is rounded to 0.1 and kept between -12 and +12. */
    inline float semitonesToKey (float hz, int root, const std::vector<int>& scale)
    {
        const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
        const int centre = (int) std::lround (midi);
        float bestNote = (float) centre, bestDist = 1.0e9f;

        for (int note = centre - 7; note <= centre + 7; ++note)
        {
            const int pc = ((note - root) % 12 + 12) % 12;

            if (std::find (scale.begin(), scale.end(), pc) == scale.end())
                continue;

            const float dist = std::abs ((float) note - midi);

            if (dist < bestDist)
            {
                bestDist = dist;
                bestNote = (float) note;
            }
        }

        const float shift = std::round ((bestNote - midi) * 10.0f) / 10.0f;
        return juce::jlimit (-12.0f, 12.0f, shift);
    }
}
