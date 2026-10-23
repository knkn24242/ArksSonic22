#pragma once

#include <cmath>

/**
    Reusable DSP building blocks for Arks Sonic 22.

    These are small, allocation-free, single-sample primitives. Circuit-specific models
    (tube stages, tone stacks, power amp, output transformer, spring reverb) are built on top of
    them in their own classes from Phase 3 onward. Keeping them here lets each circuit stage be
    developed and swapped independently (see AmpEngine).
*/
namespace Dsp
{
    inline constexpr double pi = 3.14159265358979323846;

    /** One-pole high-pass, used as a DC blocker after nonlinear stages. Cutoff in Hz.
        y[n] = a * (y[n-1] + x[n] - x[n-1]), a = 1 / (1 + 2*pi*fc/fs). */
    struct DcBlocker
    {
        float a = 0.9995f;
        float x1 = 0.0f, y1 = 0.0f;

        void setCutoff (double cutoffHz, double sampleRate)
        {
            const double w = 2.0 * pi * cutoffHz / sampleRate;
            a = (float) (1.0 / (1.0 + w));
        }

        void reset() { x1 = 0.0f; y1 = 0.0f; }

        float process (float x)
        {
            const float y = a * (y1 + x - x1);
            x1 = x;
            y1 = y;
            return y;
        }
    };

    /** First-order one-pole low-pass (RC). Reused for simple smoothing / tilt. */
    struct OnePoleLp
    {
        float a = 0.0f;     // smoothing coefficient (0 = no smoothing)
        float z = 0.0f;

        void setCutoff (double cutoffHz, double sampleRate)
        {
            const double x = std::exp (-2.0 * pi * cutoffHz / sampleRate);
            a = (float) x;
        }

        void reset (float value = 0.0f) { z = value; }

        float process (float x)
        {
            z = x + a * (z - x);
            return z;
        }
    };

    /** dB <-> linear helpers (kept here so every block uses the same convention). */
    inline float dbToGain (float db)  { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float gain) { return 20.0f * std::log10 (std::max (gain, 1.0e-9f)); }
}
