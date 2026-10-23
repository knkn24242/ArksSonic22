#pragma once

#include <cmath>

/**
    Fender passive tone stack, modelled as the real RC network (Treble / Bass / Mid).

    Two instances are used, both read from the service schematic (docs/circuit_analysis.md):

      Vintage (blackface Treble/Bass, no Mid pot - a fixed 6.8 k leg):
        R1 = 250 k Treble, R2 = 250 k Bass, R3 = 6.8 k (fixed, schematic R11),
        R4 = 100 k slope (R8), C1 = 250 pF, C2 = 0.1 uF, C3 = 0.047 uF
      Burn (TMB):
        R1 = 250 k Treble (R47, linear), R2 = 250 k Bass (R48, audio), R3 = 25 k Mid (R49, linear),
        R4 = 120 k slope (R46), C1 = 150 pF (C22), C2 = 0.15 uF (C23), C3 = 0.022 uF (C24)

    Both map onto the standard Fender tone-stack transfer function (Yeh & Smith, DAFx-06; coefficient
    form from the Faust `tonestacks` library):

        H(s) = (b3 s^3 + b2 s^2 + b1 s) / (a3 s^3 + a2 s^2 + a1 s + a0),  a0 = 1

    discretised per sample rate with the bilinear transform (3rd-order IIR). The whole passive
    network is solved, so control interaction and the network's loading are preserved. Pot tapers
    (audio = log, linear) match the schematic's A/B pot codes (APPROXIMATED mapping).
*/
class FenderToneStack
{
public:
    struct Components { double R1, R2, R3, R4, C1, C2, C3; };
    enum class Taper { linear, audio };

    /** fixedMid >= 0: the mid leg is not a control; use this fraction (Vintage 6.8 k leg -> 1.0). */
    void configure (const Components& c, Taper trebleT, Taper bassT, Taper midT, float fixedMid = -1.0f)
    {
        comp = c;
        trebleTaper = trebleT;
        bassTaper = bassT;
        midTaper = midT;
        fixedMidFrac = fixedMid;
        lastT = lastB = lastM = -1.0;
    }

    void setSampleRate (double sr)
    {
        sampleRate = sr;
        lastT = lastB = lastM = -1.0;   // force a coefficient update
        reset();
    }

    void reset() { x1 = x2 = x3 = y1 = y2 = y3 = 0.0; }

    /** treble / bass / mid in 0..1 (mid ignored when a fixed mid leg was configured). */
    void update (float treble, float bass, float mid = 1.0f)
    {
        if ((double) treble == lastT && (double) bass == lastB && (double) mid == lastM)
            return;
        lastT = treble; lastB = bass; lastM = mid;

        const double t = taper (treble, trebleTaper);
        const double l = taper (bass, bassTaper);
        const double m = fixedMidFrac >= 0.0f ? (double) fixedMidFrac : taper (mid, midTaper);

        const double R1 = comp.R1, R2 = comp.R2, R3 = comp.R3, R4 = comp.R4;
        const double C1 = comp.C1, C2 = comp.C2, C3 = comp.C3;

        const double b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
        const double b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4)
                        - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
                        + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                        + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
        const double b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                        - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
                        + t * l * C1 * C2 * C3 * R1 * R2 * R4;
        const double a0 = 1.0;
        const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4)
                        + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
        const double a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                        - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
                        + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3
                           + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
        const double a3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                        - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
                        + l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;

        bilinear (0.0, b1, b2, b3, a0, a1, a2, a3);
    }

    float process (float in)
    {
        const double x0 = (double) in;
        const double y0 = bz[0] * x0 + bz[1] * x1 + bz[2] * x2 + bz[3] * x3
                          - az[1] * y1 - az[2] * y2 - az[3] * y3;
        x3 = x2; x2 = x1; x1 = x0;
        y3 = y2; y2 = y1; y1 = y0;
        return (float) y0;
    }

private:
    static double clamp01 (double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

    static double taper (float knob, Taper t)
    {
        const double x = clamp01 ((double) knob);
        return t == Taper::audio ? std::exp ((x - 1.0) * 3.4)   // audio / log pot
                                 : (x < 1.0e-3 ? 1.0e-3 : x);   // linear pot (keep off exact 0)
    }

    void bilinear (double b0, double b1, double b2, double b3,
                   double a0, double a1, double a2, double a3)
    {
        const double c = 2.0 * sampleRate;
        const double c2 = c * c, c3 = c2 * c;

        auto expand = [c, c2, c3] (double p0, double p1, double p2, double p3, double out[4])
        {
            const double A = p3 * c3, B = p2 * c2, C = p1 * c, D = p0;
            out[0] =  A + B + C + D;
            out[1] = -3.0 * A - B + C + 3.0 * D;
            out[2] =  3.0 * A - B - C + 3.0 * D;
            out[3] = -A + B - C + D;
        };

        double N[4], M[4];
        expand (b0, b1, b2, b3, N);
        expand (a0, a1, a2, a3, M);
        const double norm = M[0];
        for (int i = 0; i < 4; ++i) { bz[i] = N[i] / norm; az[i] = M[i] / norm; }
    }

    Components comp { 250.0e3, 250.0e3, 6.8e3, 100.0e3, 250.0e-12, 100.0e-9, 47.0e-9 };
    Taper trebleTaper = Taper::audio, bassTaper = Taper::audio, midTaper = Taper::linear;
    float fixedMidFrac = 1.0f;

    double sampleRate = 48000.0;
    double bz[4] { 0, 0, 0, 0 }, az[4] { 1, 0, 0, 0 };
    double x1 = 0, x2 = 0, x3 = 0, y1 = 0, y2 = 0, y3 = 0;
    double lastT = -1.0, lastB = -1.0, lastM = -1.0;
};
