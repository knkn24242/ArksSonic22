#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "DspBlocks.h"
#include "ToneStack.h"
#include "TriodeStage.h"

#include <array>
#include <atomic>
#include <memory>

/**
    Arks Sonic 22 amplifier engine.

    Reference hardware: Fender Super-Sonic 22 Combo (service schematic 0078327000 Rev A).

        in -> Input Load -> V1-A -> [Vintage: TB tone + Fat + V1-B]  \
                                    [Burn: Gain1 -> V1-B -> Gain2 -> V2-A -> TMB tone] -> V2-B (shared)
            -> channel Volume -> FX loop -> V5 phase inverter -> 6V6 push-pull -> OT (+NFB)
            -> speaker / cabinet -> out       (spring reverb mixed in, shared)

    PHASE 4 (now): both preamp channels are real circuit models, run inside an oversampled region:
        Vintage: V1-A (12AX7) -> Fat -> Treble/Bass tone stack -> Volume -> V1-B (12AX7)
        Burn:    V1-A -> Gain1 -> V1-B -> Gain2 -> V2-A -> TMB tone stack -> V2-B -> Volume
    The power amp / output transformer / cabinet / spring reverb are still identity and arrive in
    Phase 5+. Each block above is its own swappable class (TriodeStage, FenderToneStack).

    Audio-thread rules: no allocation / locking / file I/O (oversamplers and work buffers are built in
    prepare); coefficients recomputed per (processing) sample rate; gains and bypass smoothed; the
    nonlinear stages are oversampled; mono input handled as well as stereo.
*/
class AmpEngine
{
public:
    enum class Channel { vintage = 0, burn = 1 };

    struct Params
    {
        Channel channel = Channel::vintage;

        float vintageVolume = 0.5f;
        float vintageTreble = 0.5f;
        float vintageBass   = 0.5f;
        bool  fat           = false;

        float burnGain1  = 0.5f;
        float burnGain2  = 0.5f;
        float burnTreble = 0.5f;
        float burnBass   = 0.5f;
        float burnMid    = 0.5f;
        float burnVolume = 0.5f;

        float reverb = 0.0f;

        float inputDb  = 0.0f;
        float outputDb = 0.0f;
        bool  bypass   = false;
        int   osFactor = 4;    // 1 = off; 2/4/8 = oversampling of the nonlinear stages
    };

    struct Meters
    {
        std::atomic<float> input { 0.0f }, output { 0.0f };
        std::atomic<bool>  overload { false };

        static float take (std::atomic<float>& a) { return a.exchange (0.0f); }
    };

    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const Params& params);

    Meters meters;

private:
    /** One independent mono amp path (the amp is mono; stereo in = dual mono). */
    struct Voice
    {
        TriodeStage v1a, v1b, v2a, v2b;          // 12AX7 gain stages
        FenderToneStack vintageTone, burnTone;   // Treble/Bass and TMB
        Dsp::OnePoleLp fatLp;                    // low-mid band for the Fat boost

        void prepare (double procRate);
        void reset();
        void setParams (const Params& p);
        void processBlock (float* data, int n);

    private:
        void processVintage (float* data, int n);
        void processBurn (float* data, int n);

        bool burn = false;
        // smoothed, ramped per block
        float volumeDrive = 0.0f, volumeTarget = 0.0f;
        float fatMix = 0.0f, fatTarget = 0.0f;
        float gain1 = 0.0f, gain1Target = 0.0f;
        float gain2 = 0.0f, gain2Target = 0.0f;
    };

    void rebuildVoices (double procRate);

    double sampleRate = 44100.0;
    double voiceRate  = 0.0;     // sample rate the voices are currently prepared for
    int channels = 2;
    int maxBlock = 512;

    std::array<Voice, maxChannels> voices;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;   // 2x, 4x, 8x

    juce::AudioBuffer<float> dryBuffer, workBuffer;

    juce::SmoothedValue<float> inputGain, outputGain, bypassMix;
    bool firstBlock = true;
};
