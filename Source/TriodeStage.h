#pragma once

#include "DspBlocks.h"

#include <cmath>

/**
    One 12AX7 common-cathode gain stage (physically-motivated, measurement-tunable).

    A full load-line / Koren plate-current solve is a Phase 9 refinement. For Phase 3 the stage is
    modelled as the elements that shape its sound, each from the schematic:

      - Voltage gain (small-signal), set by the plate load / cathode resistor of the stage.
      - Asymmetric soft saturation: a single triode clips harder on grid-conduction (positive grid
        swing, Miller/grid-current compression) than at cut-off, which is what gives one stage its
        even-harmonic (2nd) warmth. Modelled with a biased tanh; the bias offset sets the asymmetry.
      - Cathode-bypass capacitor: a bypassed cathode raises gain above its corner frequency
        f = 1/(2*pi*Rk*Ck). Modelled as a gentle low-frequency gain shelf.
      - Plate / Miller high-frequency roll-off.
      - DC blocker after the nonlinearity (the coupling cap to the next stage).

    Device parameters (mu/gm) are not on the schematic (INFERRED - see docs/known_unknowns.md); the
    drive / bias / gain here are tuned so the Vintage channel's headroom and break-up match a
    blackface-style clean channel, to be calibrated against hardware test-point levels in Phase 9.
*/
class TriodeStage
{
public:
    struct Design
    {
        float smallSignalGain = 1.0f;   // linear gain around the operating point
        float bias = 0.12f;             // operating-point offset -> asymmetry / 2nd harmonic
        float cathodeCornerHz = 5.0f;   // cathode-bypass shelf corner (Rk, Ck)
        float cathodeBoostDb = 0.0f;    // extra gain above the corner (0 = fully bypassed, flat)
        float plateRolloffHz = 11000.0f; // plate / Miller HF roll-off
    };

    void prepare (double sampleRate, const Design& d)
    {
        design = d;
        dc.setCutoff (5.0, sampleRate);
        plateLp.setCutoff (d.plateRolloffHz, sampleRate);
        cathodeLp.setCutoff (d.cathodeCornerHz, sampleRate);
        cathodeBoost = std::pow (10.0f, d.cathodeBoostDb / 20.0f) - 1.0f;   // extra high-shelf amount
        offset = std::tanh (d.bias);
        reset();
    }

    void reset()
    {
        dc.reset();
        plateLp.reset();
        cathodeLp.reset();
    }

    /** Extra drive multiplier (e.g. the channel Volume feeding this stage). */
    void setDrive (float d) { drive = d; }

    float process (float x)
    {
        float v = x * design.smallSignalGain * drive;

        // Cathode-bypass shelf: boost the content above the corner frequency.
        if (cathodeBoost > 0.0f)
        {
            const float low = cathodeLp.process (v);
            v += cathodeBoost * (v - low);
        }

        // Asymmetric triode saturation (biased tanh, DC removed).
        float y = std::tanh (v + design.bias) - offset;

        y = plateLp.process (y);
        return dc.process (y);
    }

private:
    Design design;
    Dsp::DcBlocker dc;
    Dsp::OnePoleLp plateLp, cathodeLp;
    float drive = 1.0f;
    float offset = 0.0f;
    float cathodeBoost = 0.0f;
};
