#pragma once

#include <algorithm>
#include <cmath>

/** Small filter and envelope helpers shared by the newer KNG plugins. */
namespace KngDsp
{
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset()   { z1 = z2 = 0.0f; }

        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void lowPass (double sr, double f, double q = 0.7071)
        {
            const auto w = setup (sr, f, q);
            set ((1.0 - w.cs) * 0.5, 1.0 - w.cs, (1.0 - w.cs) * 0.5, 1.0 + w.alpha, -2.0 * w.cs, 1.0 - w.alpha);
        }

        void highPass (double sr, double f, double q = 0.7071)
        {
            const auto w = setup (sr, f, q);
            set ((1.0 + w.cs) * 0.5, -(1.0 + w.cs), (1.0 + w.cs) * 0.5, 1.0 + w.alpha, -2.0 * w.cs, 1.0 - w.alpha);
        }

        void bandPass (double sr, double f, double q)
        {
            const auto w = setup (sr, f, q);
            set (w.alpha, 0.0, -w.alpha, 1.0 + w.alpha, -2.0 * w.cs, 1.0 - w.alpha);
        }

        void peak (double sr, double f, double gainDb, double q)
        {
            const auto w = setup (sr, f, q);
            const double A = std::pow (10.0, gainDb / 40.0);
            set (1.0 + w.alpha * A, -2.0 * w.cs, 1.0 - w.alpha * A, 1.0 + w.alpha / A, -2.0 * w.cs, 1.0 - w.alpha / A);
        }

        void lowShelf (double sr, double f, double gainDb, double q = 0.7071)
        {
            const auto w = setup (sr, f, q);
            const double A = std::pow (10.0, gainDb / 40.0), sq = 2.0 * std::sqrt (A) * w.alpha;
            set (A * ((A + 1.0) - (A - 1.0) * w.cs + sq), 2.0 * A * ((A - 1.0) - (A + 1.0) * w.cs),
                 A * ((A + 1.0) - (A - 1.0) * w.cs - sq), (A + 1.0) + (A - 1.0) * w.cs + sq,
                 -2.0 * ((A - 1.0) + (A + 1.0) * w.cs), (A + 1.0) + (A - 1.0) * w.cs - sq);
        }

        void highShelf (double sr, double f, double gainDb, double q = 0.7071)
        {
            const auto w = setup (sr, f, q);
            const double A = std::pow (10.0, gainDb / 40.0), sq = 2.0 * std::sqrt (A) * w.alpha;
            set (A * ((A + 1.0) + (A - 1.0) * w.cs + sq), -2.0 * A * ((A - 1.0) + (A + 1.0) * w.cs),
                 A * ((A + 1.0) + (A - 1.0) * w.cs - sq), (A + 1.0) - (A - 1.0) * w.cs + sq,
                 2.0 * ((A - 1.0) - (A + 1.0) * w.cs), (A + 1.0) - (A - 1.0) * w.cs - sq);
        }

    private:
        struct W { double cs, sn, alpha; };

        static W setup (double sr, double f, double q)
        {
            f = std::clamp (f, 10.0, sr * 0.45);
            const double w0 = 2.0 * 3.14159265358979323846 * f / sr;
            return { std::cos (w0), std::sin (w0), std::sin (w0) / (2.0 * std::max (0.05, q)) };
        }

        void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
        {
            b0 = (float) (nb0 / a0);
            b1 = (float) (nb1 / a0);
            b2 = (float) (nb2 / a0);
            a1 = (float) (na1 / a0);
            a2 = (float) (na2 / a0);
        }
    };

    /** Follows how loud a signal is. attack and release come from coef(). */
    struct Follower
    {
        float env = 0.0f;

        void reset()   { env = 0.0f; }

        float process (float x, float attack, float release)
        {
            x = std::abs (x);
            env += (x > env ? attack : release) * (x - env);
            return env;
        }

        static float coef (double ms, double sr)
        {
            return (float) (1.0 - std::exp (-1.0 / std::max (1.0, ms * 0.001 * sr)));
        }
    };
}
