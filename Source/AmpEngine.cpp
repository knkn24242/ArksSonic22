#include "AmpEngine.h"

namespace
{
    // Voicing constants. Physically-motivated; tuned for blackface-style Vintage headroom and a
    // saturated Burn. Absolute operating points to be calibrated against hardware in Phase 9.

    // Vintage
    constexpr float kFatCutoffHz = 400.0f;
    constexpr float kFatGain     = 0.6f;
    constexpr float kVinMakeup   = 9.0f;     // recovers the TB tone-stack insertion loss
    constexpr float kVinVolMax   = 4.0f;
    constexpr float kVinPost     = 0.5f;

    // Burn (three cascaded gain stages -> TMB tone -> driver -> Volume)
    constexpr float kG1Max       = 7.0f;     // Gain 1 -> drive into V1-B
    constexpr float kG2Max       = 6.0f;     // Gain 2 -> drive into V2-A
    constexpr float kBurnMakeup  = 11.0f;    // recovers the TMB tone-stack insertion loss
    constexpr float kBurnVolMax  = 2.5f;
    constexpr float kBurnPost    = 0.35f;

    int osIndex (int factor) { return factor >= 8 ? 2 : (factor >= 4 ? 1 : 0); }

    float taperAudio (float x) { return x * x; }   // audio-ish taper for Volume / Gain pots
}

//==============================================================================
void AmpEngine::Voice::prepare (double procRate)
{
    TriodeStage::Design a;   // V1-A (shared input stage)
    a.smallSignalGain = 1.4f;  a.bias = 0.12f;  a.cathodeCornerHz = 5.0f;   a.plateRolloffHz = 11000.0f;
    v1a.prepare (procRate, a);

    TriodeStage::Design b;   // V1-B
    b.smallSignalGain = 1.2f;  b.bias = 0.10f;  b.cathodeCornerHz = 48.0f;  b.plateRolloffHz = 12000.0f;
    v1b.prepare (procRate, b);

    TriodeStage::Design c;   // V2-A (Burn third gain stage)
    c.smallSignalGain = 1.2f;  c.bias = 0.09f;  c.cathodeCornerHz = 48.0f;  c.plateRolloffHz = 12000.0f;
    v2a.prepare (procRate, c);

    TriodeStage::Design d;   // V2-B (shared driver)
    d.smallSignalGain = 1.1f;  d.bias = 0.08f;  d.cathodeCornerHz = 5.0f;   d.plateRolloffHz = 12000.0f;
    v2b.prepare (procRate, d);

    vintageTone.configure ({ 250.0e3, 250.0e3, 6.8e3, 100.0e3, 250.0e-12, 100.0e-9, 47.0e-9 },
                           FenderToneStack::Taper::audio, FenderToneStack::Taper::audio,
                           FenderToneStack::Taper::linear, 1.0f);        // fixed 6.8 k mid leg
    vintageTone.setSampleRate (procRate);

    burnTone.configure ({ 250.0e3, 250.0e3, 25.0e3, 120.0e3, 150.0e-12, 150.0e-9, 22.0e-9 },
                        FenderToneStack::Taper::linear, FenderToneStack::Taper::audio,
                        FenderToneStack::Taper::linear, -1.0f);          // Mid is a real control
    burnTone.setSampleRate (procRate);

    fatLp.setCutoff (kFatCutoffHz, procRate);
    reset();
}

void AmpEngine::Voice::reset()
{
    v1a.reset(); v1b.reset(); v2a.reset(); v2b.reset();
    vintageTone.reset(); burnTone.reset(); fatLp.reset();
    volumeDrive = fatMix = gain1 = gain2 = 0.0f;   // ramped up on the first block
}

void AmpEngine::Voice::setParams (const Params& p)
{
    burn = (p.channel == Channel::burn);
    if (burn)
    {
        burnTone.update (p.burnTreble, p.burnBass, p.burnMid);
        gain1Target  = 0.5f + taperAudio (p.burnGain1) * kG1Max;
        gain2Target  = 0.5f + taperAudio (p.burnGain2) * kG2Max;
        volumeTarget = taperAudio (p.burnVolume) * kBurnVolMax;
    }
    else
    {
        vintageTone.update (p.vintageTreble, p.vintageBass);
        volumeTarget = taperAudio (p.vintageVolume) * kVinVolMax;
        fatTarget = p.fat ? 1.0f : 0.0f;
    }
}

void AmpEngine::Voice::processBlock (float* data, int n)
{
    if (burn) processBurn (data, n);
    else      processVintage (data, n);
}

void AmpEngine::Voice::processVintage (float* data, int n)
{
    const float volInc = n > 0 ? (volumeTarget - volumeDrive) / (float) n : 0.0f;
    const float fatInc = n > 0 ? (fatTarget - fatMix) / (float) n : 0.0f;

    for (int i = 0; i < n; ++i)
    {
        volumeDrive += volInc;
        fatMix += fatInc;

        float s = v1a.process (data[i]);
        s += fatMix * kFatGain * fatLp.process (s);
        s = vintageTone.process (s);
        s *= kVinMakeup * volumeDrive;
        s = v1b.process (s);
        data[i] = s * kVinPost;
    }
    volumeDrive = volumeTarget;
    fatMix = fatTarget;
}

void AmpEngine::Voice::processBurn (float* data, int n)
{
    const float g1Inc  = n > 0 ? (gain1Target - gain1) / (float) n : 0.0f;
    const float g2Inc  = n > 0 ? (gain2Target - gain2) / (float) n : 0.0f;
    const float volInc = n > 0 ? (volumeTarget - volumeDrive) / (float) n : 0.0f;

    for (int i = 0; i < n; ++i)
    {
        gain1 += g1Inc;
        gain2 += g2Inc;
        volumeDrive += volInc;

        float s = v1a.process (data[i]);          // shared input stage
        v1b.setDrive (gain1);
        s = v1b.process (s);                       // Gain 1 -> V1-B
        v2a.setDrive (gain2);
        s = v2a.process (s);                       // Gain 2 -> V2-A
        s = burnTone.process (s);                  // TMB tone stack
        s *= kBurnMakeup;
        s = v2b.process (s);                       // shared driver V2-B
        data[i] = s * volumeDrive * kBurnPost;     // Burn Volume
    }
    gain1 = gain1Target;
    gain2 = gain2Target;
    volumeDrive = volumeTarget;
}

//==============================================================================
void AmpEngine::prepare (double newSampleRate, int maxBlockSize, int numChannels)
{
    sampleRate = newSampleRate;
    channels   = juce::jlimit (1, maxChannels, numChannels);
    maxBlock   = juce::jmax (1, maxBlockSize);

    inputGain.reset (sampleRate, 0.01);
    outputGain.reset (sampleRate, 0.01);
    bypassMix.reset (sampleRate, 0.02);

    dryBuffer.setSize (channels, maxBlock);
    workBuffer.setSize (channels, maxBlock);

    using juce::dsp::Oversampling;
    for (int f = 1; f <= 3; ++f)
    {
        auto os = std::make_unique<Oversampling<float>> ((size_t) channels, (size_t) f,
                                                         Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing ((size_t) maxBlock);
        os->reset();
        oversamplers[(size_t) (f - 1)] = std::move (os);
    }

    voiceRate = 0.0;
    rebuildVoices (sampleRate);
    firstBlock = true;
}

void AmpEngine::rebuildVoices (double procRate)
{
    voiceRate = procRate;
    for (auto& v : voices)
        v.prepare (procRate);
}

void AmpEngine::reset()
{
    for (auto& v : voices)
        v.reset();
    for (auto& os : oversamplers)
        if (os != nullptr)
            os->reset();
}

void AmpEngine::process (juce::AudioBuffer<float>& buffer, const Params& p)
{
    const int n  = buffer.getNumSamples();
    const int nc = juce::jmin (buffer.getNumChannels(), channels);

    inputGain.setTargetValue (Dsp::dbToGain (p.inputDb));
    outputGain.setTargetValue (Dsp::dbToGain (p.outputDb));
    bypassMix.setTargetValue (p.bypass ? 0.0f : 1.0f);

    if (firstBlock)
    {
        inputGain.setCurrentAndTargetValue (Dsp::dbToGain (p.inputDb));
        outputGain.setCurrentAndTargetValue (Dsp::dbToGain (p.outputDb));
        bypassMix.setCurrentAndTargetValue (p.bypass ? 0.0f : 1.0f);
        firstBlock = false;
    }

    // Both channels are nonlinear; oversample the preamp.
    const int    osf      = juce::jlimit (1, 8, p.osFactor);
    const double procRate = sampleRate * osf;
    if (procRate != voiceRate)
        rebuildVoices (procRate);

    for (int ch = 0; ch < channels; ++ch)
        voices[(size_t) ch].setParams (p);

    // 1) capture dry + apply input gain into the work buffer.
    float inPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float ig = inputGain.getNextValue();
        for (int ch = 0; ch < channels; ++ch)
        {
            const float x = ch < nc ? buffer.getSample (ch, i) : 0.0f;
            inPeak = juce::jmax (inPeak, std::abs (x));
            dryBuffer.setSample (ch, i, x);
            workBuffer.setSample (ch, i, x * ig);
        }
    }

    // 2) nonlinear preamp, inside the oversampled region.
    if (osf == 1)
    {
        for (int ch = 0; ch < channels; ++ch)
            voices[(size_t) ch].processBlock (workBuffer.getWritePointer (ch), n);
    }
    else
    {
        auto* os = oversamplers[(size_t) osIndex (osf)].get();
        juce::dsp::AudioBlock<float> block (workBuffer);
        auto sub = block.getSubsetChannelBlock (0, (size_t) channels).getSubBlock (0, (size_t) n);
        auto up = os->processSamplesUp (sub);
        const int upN = (int) up.getNumSamples();
        for (int ch = 0; ch < channels; ++ch)
            voices[(size_t) ch].processBlock (up.getChannelPointer ((size_t) ch), upN);
        os->processSamplesDown (sub);
    }

    // 3) output gain + click-free bypass crossfade + meters.
    float outPeak = 0.0f;
    bool overload = false;
    for (int i = 0; i < n; ++i)
    {
        const float og  = outputGain.getNextValue();
        const float mix = bypassMix.getNextValue();
        for (int ch = 0; ch < nc; ++ch)
        {
            const float wet = workBuffer.getSample (ch, i) * og;
            const float dry = dryBuffer.getSample (ch, i);
            const float y   = dry + mix * (wet - dry);
            buffer.setSample (ch, i, y);
            outPeak = juce::jmax (outPeak, std::abs (y));
            overload = overload || std::abs (y) >= 1.0f;
        }
    }

    meters.input.store  (juce::jmax (meters.input.load(),  inPeak));
    meters.output.store (juce::jmax (meters.output.load(), outPeak));
    if (overload)
        meters.overload.store (true);
}
