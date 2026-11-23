#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <cmath>
#include <vector>

/** Finds the notes in one melody line, like a vocal or a lead. One note at a time, no chords. */
namespace KngPitch
{
    struct Note
    {
        double start = 0.0;    // seconds from the start of the part that was read
        double length = 0.0;
        int pitch = 60;
        int velocity = 100;
    };

    constexpr int window = 512;   // samples each pitch guess looks at (after shrinking)
    constexpr int hop = 128;      // samples between guesses

    inline int frameSize (double rate)
    {
        return window + juce::jmin (window - 2, (int) (rate / 70.0)) + 2;
    }

    /** One pitch guess (the YIN method). Returns a MIDI note as a decimal, or -1 when nothing steady is there. */
    inline float guessPitch (const float* x, double rate, std::vector<float>& cmnd)
    {
        const int tauMin = juce::jmax (2, (int) (rate / 1000.0));
        const int tauMax = juce::jmin (window - 2, (int) (rate / 70.0));

        if (tauMax <= tauMin + 2)
            return -1.0f;

        cmnd.assign ((size_t) tauMax + 2, 1.0f);
        float running = 0.0f;

        for (int tau = 1; tau <= tauMax + 1; ++tau)
        {
            float d = 0.0f;

            for (int j = 0; j < window; ++j)
            {
                const float diff = x[j] - x[j + tau];
                d += diff * diff;
            }

            running += d;
            cmnd[(size_t) tau] = running > 0.0f ? d * (float) tau / running : 1.0f;
        }

        int best = -1;

        // The first clear dip is the pitch
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < 0.15f)
            {
                while (tau + 1 <= tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                    ++tau;

                best = tau;
                break;
            }
        }

        // No clear dip: take the deepest one, unless it is weak
        if (best < 0)
        {
            float lowest = 1.0f;

            for (int tau = tauMin; tau <= tauMax; ++tau)
            {
                if (cmnd[(size_t) tau] < lowest)
                {
                    lowest = cmnd[(size_t) tau];
                    best = tau;
                }
            }

            if (best < 0 || lowest > 0.3f)
                return -1.0f;
        }

        // A curve through the three points around the dip gives a finer pitch
        double tauBest = (double) best;
        const double a = cmnd[(size_t) best - 1];
        const double b = cmnd[(size_t) best];
        const double c = cmnd[(size_t) best + 1];
        const double denom = a - 2.0 * b + c;

        if (std::abs (denom) > 1.0e-9)
            tauBest += 0.5 * (a - c) / denom;

        const double hz = rate / tauBest;

        if (hz < 70.0 || hz > 1000.0)
            return -1.0f;

        return (float) (69.0 + 12.0 * std::log2 (hz / 440.0));
    }

    /** Reads lengthSeconds of the file from startSeconds and returns the notes found, in seconds from startSeconds. */
    inline std::vector<Note> findNotes (juce::AudioFormatReader& reader, double startSeconds, double lengthSeconds)
    {
        std::vector<Note> notes;

        if (reader.sampleRate <= 0.0 || reader.numChannels == 0)
            return notes;

        // Shrink to about 12 kHz first. A voice does not need more, and it is much faster.
        const int shrink = juce::jmax (1, (int) std::round (reader.sampleRate / 12000.0));
        const double rate = reader.sampleRate / (double) shrink;

        const juce::int64 first = (juce::int64) (startSeconds * reader.sampleRate);
        const juce::int64 last  = juce::jmin (reader.lengthInSamples, first + (juce::int64) (lengthSeconds * reader.sampleRate));

        if (last <= first)
            return notes;

        std::vector<float> mono;
        mono.reserve ((size_t) ((last - first) / shrink) + 1);

        const int chunk = shrink * 4096;
        juce::AudioBuffer<float> buffer ((int) reader.numChannels, chunk);

        for (juce::int64 pos = first; pos + shrink <= last; pos += chunk)
        {
            const int n = (int) juce::jmin ((juce::int64) chunk, last - pos);
            buffer.clear();
            reader.read (&buffer, 0, n, pos, true, true);

            for (int i = 0; i + shrink <= n; i += shrink)
            {
                float sum = 0.0f;

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    const float* d = buffer.getReadPointer (ch, i);

                    for (int k = 0; k < shrink; ++k)
                        sum += d[k];
                }

                mono.push_back (sum / (float) (shrink * buffer.getNumChannels()));
            }
        }

        const int need = frameSize (rate);

        if ((int) mono.size() < need + hop)
            return notes;

        const int numFrames = ((int) mono.size() - need) / hop + 1;
        std::vector<float> level ((size_t) numFrames, 0.0f);
        std::vector<float> pitch ((size_t) numFrames, -1.0f);
        std::vector<float> cmnd;
        float peak = 0.0f;

        for (int f = 0; f < numFrames; ++f)
        {
            const float* x = mono.data() + (size_t) f * (size_t) hop;
            double sum = 0.0;

            for (int j = 0; j < window; ++j)
                sum += (double) x[j] * (double) x[j];

            level[(size_t) f] = (float) std::sqrt (sum / (double) window);
            peak = juce::jmax (peak, level[(size_t) f]);
        }

        if (peak < 1.0e-4f)
            return notes;

        // Quiet stretches count as silence
        for (int f = 0; f < numFrames; ++f)
            if (level[(size_t) f] >= peak * 0.08f)
                pitch[(size_t) f] = guessPitch (mono.data() + (size_t) f * (size_t) hop, rate, cmnd);

        // One odd guess (often an octave jump) should not cut a note in two
        auto smooth = pitch;

        for (int f = 0; f < numFrames; ++f)
        {
            if (pitch[(size_t) f] < 0.0f)
                continue;

            std::vector<float> around;

            for (int k = juce::jmax (0, f - 2); k <= juce::jmin (numFrames - 1, f + 2); ++k)
                if (pitch[(size_t) k] >= 0.0f)
                    around.push_back (pitch[(size_t) k]);

            if (around.size() >= 3)
            {
                std::nth_element (around.begin(), around.begin() + (long) (around.size() / 2), around.end());
                smooth[(size_t) f] = around[around.size() / 2];
            }
        }

        const double frameLen = (double) hop / rate;
        auto centre = [&] (int f) { return ((double) f * (double) hop + (double) window * 0.5) / rate; };

        bool open = false;
        int firstFrame = 0, lastFrame = 0, voiced = 0, gap = 0;
        double pitchSum = 0.0;
        float loudest = 0.0f;

        auto finish = [&]
        {
            if (open && voiced > 0)
            {
                Note n;
                n.start = juce::jmax (0.0, centre (firstFrame) - frameLen * 0.5);
                n.length = centre (lastFrame) + frameLen * 0.5 - n.start;
                n.pitch = juce::jlimit (0, 127, (int) std::lround (pitchSum / (double) voiced));
                n.velocity = juce::jlimit (30, 127, (int) (40.0f + 87.0f * loudest / peak));

                // Anything shorter than 60 ms is a blip, not a note
                if (n.length >= 0.06)
                    notes.push_back (n);
            }

            open = false;
        };

        for (int f = 0; f < numFrames; ++f)
        {
            const float p = smooth[(size_t) f];

            if (p < 0.0f)
            {
                // A short dropout does not end the note
                if (open && ++gap > 3)
                    finish();

                continue;
            }

            // The same note until the pitch moves more than three quarters of a semitone, so vibrato stays one note
            if (open && std::abs ((double) p - pitchSum / (double) voiced) > 0.75)
                finish();

            if (! open)
            {
                open = true;
                firstFrame = f;
                voiced = 0;
                pitchSum = 0.0;
                loudest = 0.0f;
            }

            lastFrame = f;
            ++voiced;
            pitchSum += (double) p;
            gap = 0;
            loudest = juce::jmax (loudest, level[(size_t) f]);
        }

        finish();
        return notes;
    }
}
