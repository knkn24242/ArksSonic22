// Offline checks on the real processor:
//   - Bypass is a bit-exact passthrough in mono, stereo and mono->stereo, at 44.1/48/88.2/96 kHz,
//     even with Input/Output gain set (the crossfade settles on the first block, so it is exact)
//   - mono in -> stereo out fans the mono signal to both outputs (checked on Bypass)
//   - zero latency; state save/restore round trip (every parameter + the selected preset)
// (From Phase 3 the Vintage channel colours the signal, so only Bypass is a bit-exact passthrough.)

#include "PluginProcessor.h"

#include <cmath>
#include <utility>
#include <cstdio>

namespace
{
    int failures = 0;

    void check (bool ok, const char* what)
    {
        std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what);
        if (! ok) ++failures;
    }

    void setParam (Sonic22Processor& p, const char* id, float v)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (v));
    }

    // inChannels/outChannels let us exercise mono, stereo and mono->stereo.
    bool runPassthrough (double sampleRate, int inChannels, int outChannels, int blockSize,
                         bool bypass, float inputDb, float outputDb)
    {
        Sonic22Processor proc;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add  (inChannels  == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        layout.outputBuses.add (outChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        if (! proc.setBusesLayout (layout))
            return false;

        setParam (proc, ParamIDs::bypass, bypass ? 1.0f : 0.0f);
        setParam (proc, ParamIDs::input, inputDb);
        setParam (proc, ParamIDs::output, outputDb);

        proc.setRateAndBufferSizeDetails (sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        juce::Random rng (1234);
        const int bufCh = juce::jmax (inChannels, outChannels);
        juce::AudioBuffer<float> buffer (bufCh, blockSize), reference (bufCh, blockSize);
        juce::MidiBuffer midi;

        for (int b = 0; b < 200; ++b)
        {
            for (int ch = 0; ch < bufCh; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    // Only input channels carry signal; extra buffer channels start silent.
                    const float s = ch < inChannels
                                    ? (float) (0.5 * std::sin (0.01 * (b * blockSize + i) * (ch + 1))) + 0.1f * (rng.nextFloat() - 0.5f)
                                    : 0.0f;
                    buffer.setSample (ch, i, s);
                    reference.setSample (ch, i, s);
                }

            proc.processBlock (buffer, midi);

            for (int ch = 0; ch < outChannels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    // mono -> stereo: the output copies input channel 0.
                    const int srcCh = ch < inChannels ? ch : 0;
                    if (buffer.getSample (ch, i) != reference.getSample (srcCh, i))
                        return false;
                }
        }

        return proc.getLatencySamples() == 0;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
        for (int bs : { 64, 512, 1024 })
        {
            auto name = [sr, bs] (const char* what) {
                return juce::String (what) + " " + juce::String (sr / 1000.0, 1) + " kHz, block " + juce::String (bs);
            };
            // Bypass: always bit-exact, even with gains set.
            check (runPassthrough (sr, 1, 1, bs, true, 6.0f, -4.0f), name ("bypass mono").toRawUTF8());
            check (runPassthrough (sr, 2, 2, bs, true, 6.0f, -4.0f), name ("bypass stereo").toRawUTF8());
            check (runPassthrough (sr, 1, 2, bs, true, 0.0f, 0.0f),  name ("bypass mono->stereo").toRawUTF8());
        }

    // State save / restore round trip (every parameter + the selected preset).
    {
        Sonic22Processor a, b;
        a.applyPreset (4);   // Burn Lead, then edit on top of it
        const std::pair<const char*, float> values[] = {
            { ParamIDs::channel, 1.0f }, { ParamIDs::vintageVolume, 63.0f }, { ParamIDs::vintageTreble, 44.0f },
            { ParamIDs::vintageBass, 71.0f }, { ParamIDs::fat, 1.0f },
            { ParamIDs::burnGain1, 82.0f }, { ParamIDs::burnGain2, 55.0f }, { ParamIDs::burnTreble, 48.0f },
            { ParamIDs::burnBass, 40.0f }, { ParamIDs::burnMid, 66.0f }, { ParamIDs::burnVolume, 51.0f },
            { ParamIDs::reverb, 33.0f }, { ParamIDs::input, -3.5f }, { ParamIDs::output, 2.5f },
            { ParamIDs::oversampling, 2.0f }, { ParamIDs::bypass, 1.0f } };
        for (auto& [id, v] : values)
        {
            auto* p = a.apvts.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (v));
        }

        juce::MemoryBlock state;
        a.getStateInformation (state);
        b.setStateInformation (state.getData(), (int) state.getSize());

        bool allRestored = true;
        for (auto& [id, v] : values)
            allRestored = allRestored && std::abs (b.apvts.getRawParameterValue (id)->load() - v) < 0.01f;
        check (allRestored, "state restore: all parameters");
        check (b.getCurrentProgram() == 4, "state restore: selected preset");

        b.setCurrentProgram (4);
        check (std::abs (b.apvts.getRawParameterValue (ParamIDs::burnGain1)->load() - 82.0f) < 0.01f,
               "re-sent current program does not overwrite restored edits");
        b.setCurrentProgram (3);
        check (std::abs (b.apvts.getRawParameterValue (ParamIDs::burnGain1)->load() - Presets::all[3].burnGain1) < 0.01f,
               "choosing another program loads it");
        check (b.getBypassParameter() == b.apvts.getParameter (ParamIDs::bypass), "host bypass parameter exposed");
    }

    // mono -> stereo must be accepted; stereo -> mono must be refused.
    {
        Sonic22Processor proc;
        juce::AudioProcessor::BusesLayout ms;
        ms.inputBuses.add (juce::AudioChannelSet::mono());
        ms.outputBuses.add (juce::AudioChannelSet::stereo());
        check (proc.checkBusesLayoutSupported (ms), "mono->stereo layout accepted");

        juce::AudioProcessor::BusesLayout sm;
        sm.inputBuses.add (juce::AudioChannelSet::stereo());
        sm.outputBuses.add (juce::AudioChannelSet::mono());
        check (! proc.checkBusesLayoutSupported (sm), "stereo->mono layout refused");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
