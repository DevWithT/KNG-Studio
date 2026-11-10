#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

/** The sound engine pieces: anti-aliased oscillators, envelopes, an analog-style filter, and a voice. */
namespace KngSynth
{
    constexpr int maxUnison = 7;

    // Smooths the sharp corners of saw and square waves so high notes don't alias
    inline float polyBlep (double t, double dt)
    {
        if (t < dt)
        {
            t /= dt;
            return (float) (t + t - t * t - 1.0);
        }

        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return (float) (t * t + t + t + 1.0);
        }

        return 0.0f;
    }

    inline float oscSample (int wave, double phase, double dt)
    {
        switch (wave)
        {
            case 0:   // saw
                return (float) (2.0 * phase - 1.0) - polyBlep (phase, dt);

            case 1:   // square
            {
                float v = phase < 0.5 ? 1.0f : -1.0f;
                v += polyBlep (phase, dt);
                v -= polyBlep (std::fmod (phase + 0.5, 1.0), dt);
                return v;
            }

            case 2:   // triangle
                return (float) (4.0 * std::abs (phase - 0.5) - 1.0);

            default:  // sine
                return (float) std::sin (phase * juce::MathConstants<double>::twoPi);
        }
    }

    //==============================================================================
    struct Envelope
    {
        enum Stage { idle, attack, decay, sustain, release };

        Stage stage = idle;
        float level = 0.0f;

        void noteOn()               { stage = attack; }
        void noteOff()              { if (stage != idle) stage = release; }
        bool isActive() const       { return stage != idle; }

        // Reaches about 99% of the way in the given time
        static float coef (float seconds, float sampleRate)
        {
            return 1.0f - std::exp (-4.6f / juce::jmax (1.0f, seconds * sampleRate));
        }

        float process (float a, float d, float s, float r, float sampleRate)
        {
            switch (stage)
            {
                case attack:
                    level += 1.0f / juce::jmax (1.0f, a * sampleRate);

                    if (level >= 1.0f)
                    {
                        level = 1.0f;
                        stage = decay;
                    }
                    break;

                case decay:
                    level += (s - level) * coef (d, sampleRate);

                    if (std::abs (level - s) < 0.0005f)
                    {
                        level = s;
                        stage = sustain;
                    }
                    break;

                case sustain:
                    level = s;
                    break;

                case release:
                    level += (0.0f - level) * coef (r, sampleRate);

                    if (level < 0.0005f)
                    {
                        level = 0.0f;
                        stage = idle;
                    }
                    break;

                default:
                    break;
            }

            return level;
        }
    };

    //==============================================================================
    // State-variable filter (topology-preserving): stays stable and smooth under fast sweeps
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f;

        void reset()   { ic1 = ic2 = 0.0f; }

        float process (float x, float g, float k, int type)
        {
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;

            if (type == 0) return v2;                  // low pass
            if (type == 1) return x - k * v1 - v2;     // high pass
            return v1;                                 // band pass
        }
    };

    //==============================================================================
    struct Voice
    {
        bool active = false;
        int note = 60;
        float velocity = 1.0f;
        juce::uint32 age = 0;

        double pitch = 60.0, targetPitch = 60.0;
        std::array<double, maxUnison> phaseA {}, phaseB {};
        double subPhase = 0.0;

        Envelope amp, filt;
        Svf filterL, filterR;
        juce::Random random;

        void start (int newNote, float newVelocity, bool retrigger, bool slide, juce::uint32 order)
        {
            note = newNote;
            velocity = newVelocity;
            targetPitch = newNote;
            age = order;

            if (! slide)
                pitch = newNote;

            if (! active)
            {
                // Fresh voice: random start phases make unison sound wide right away
                for (auto& p : phaseA) p = random.nextDouble();
                for (auto& p : phaseB) p = random.nextDouble();
                subPhase = 0.0;
                filterL.reset();
                filterR.reset();
                amp = {};
                filt = {};
            }

            if (retrigger || ! active)
            {
                amp.noteOn();
                filt.noteOn();
            }

            active = true;
        }
    };
}
