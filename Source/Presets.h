#pragma once

#include <array>

/**
    Factory presets, exposed to the host as programs and in the plugin's own preset menu.

    PHASE 2: presets set every control to a stable starting point for each channel. They do not yet
    change the sound (the circuit model is identity until Phase 3); the values and plumbing are in
    place so later phases only have to make the model respond to them.

    Values are 0-100 for knobs, channel is 0 = Vintage / 1 = Burn, fat is 0/1.
*/
namespace Presets
{
    struct Preset
    {
        const char* name;
        int   channel;        // 0 = Vintage, 1 = Burn
        float vintageVolume, vintageTreble, vintageBass;
        int   fat;            // 0/1 (Vintage voicing switch)
        float burnGain1, burnGain2, burnTreble, burnBass, burnMid, burnVolume;
        float reverb;
    };

    inline constexpr std::array<Preset, 6> all {{
        //  name                 ch  vVol vTre vBas fat  g1   g2   bTre bBas bMid bVol  rvb
        { "Vintage Clean",        0,  50,  60,  50,  0,   40,  40,  50,  50,  50,  50,   20 },
        { "Vintage Fat",          0,  55,  55,  60,  1,   40,  40,  50,  50,  50,  50,   25 },
        { "Vintage Edge",         0,  75,  65,  50,  0,   40,  40,  50,  50,  50,  50,   15 },
        { "Burn Crunch",          1,  50,  60,  50,  0,   55,  45,  60,  45,  55,  50,   15 },
        { "Burn Lead",            1,  50,  60,  50,  0,   75,  65,  60,  40,  55,  45,   25 },
        { "Burn Saturated",       1,  50,  60,  50,  0,   90,  80,  55,  45,  50,  40,   20 },
    }};
}
