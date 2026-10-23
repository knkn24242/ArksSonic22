// CPU load of Sonic22Processor::processBlock.
// For 44.1/48/88.2/96 kHz x mono/stereo x block 64/256/512/1024 it processes a bass+kick signal and
// reports the average and worst block time as a share of one CPU core. PHASE 2: the model is identity,
// so these are a baseline; later phases (oversampled nonlinear stages) will raise it.
//
// Usage: CpuBenchmark [seconds per case, default 20]

#include "PluginProcessor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;
    struct Result { double averagePercent, p999Percent, worstPercent, microsPerBlock; };

    void setParam (Sonic22Processor& p, const char* id, float v)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (v));
    }

    std::vector<float> makeSource (double sr, int n)
    {
        std::vector<float> x ((size_t) n);
        juce::Random rng (5);
        double kickPhase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            double s = 0.0;
            for (int k = 1; k * 82.4 < 6000.0; ++k)
                s += 0.2 / k * std::sin (juce::MathConstants<double>::twoPi * k * 82.4 * i / sr);   // E2 bass
            const double t = std::fmod (i / sr, 0.5);
            if (t < 1.0 / sr) kickPhase = 0.0;
            kickPhase += juce::MathConstants<double>::twoPi * (48.0 + 110.0 * std::exp (-t / 0.035)) / sr;
            s += 0.5 * std::exp (-t / 0.12) * std::sin (kickPhase);
            x[(size_t) i] = (float) s + 0.002f * (rng.nextFloat() - 0.5f);
        }
        return x;
    }

    enum class Mode { defaults, everything };

    Result measure (double sr, int channels, int blockSize, double seconds, Mode mode, const std::vector<float>& source)
    {
        Sonic22Processor proc;
        const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (set);
        layout.outputBuses.add (set);
        proc.setBusesLayout (layout);

        if (mode == Mode::everything)
        {
            setParam (proc, ParamIDs::channel, 0.0f);        // Vintage (nonlinear, oversampled)
            setParam (proc, ParamIDs::oversampling, 3.0f);   // 8x
            setParam (proc, ParamIDs::vintageVolume, 90.0f); // driven into break-up
            setParam (proc, ParamIDs::input, 6.0f);
        }

        proc.setRateAndBufferSizeDetails (sr, blockSize);
        proc.prepareToPlay (sr, blockSize);

        juce::AudioBuffer<float> buffer (channels, blockSize);
        juce::MidiBuffer midi;
        const int blocks = (int) (seconds * sr / blockSize);
        const double blockSeconds = blockSize / sr;
        double total = 0.0, worst = 0.0;
        std::vector<double> times;
        times.reserve ((size_t) blocks);
        size_t pos = 0;

        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (ch, i, source[(pos + (size_t) i) % source.size()] * (ch == 0 ? 1.0f : 0.9f));
            pos += (size_t) blockSize;

            const auto t0 = Clock::now();
            proc.processBlock (buffer, midi);
            const double dt = std::chrono::duration<double> (Clock::now() - t0).count();

            if (b >= 10)
            {
                total += dt;
                worst = std::max (worst, dt);
                times.push_back (dt);
            }
        }

        const int measured = blocks - 10;
        std::sort (times.begin(), times.end());
        const double p999 = times[std::min (times.size() - 1, (size_t) ((double) times.size() * 0.999))];
        return { 100.0 * total / (measured * blockSeconds), 100.0 * p999 / blockSeconds,
                 100.0 * worst / blockSeconds, 1.0e6 * total / measured };
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    const double seconds = argc > 1 ? std::atof (argv[1]) : 20.0;

    std::printf ("Arks Sonic 22 CPU benchmark (%.0f s of audio per case)\n", seconds);
    std::printf ("CPU: %s, %d logical cores | %s\n\n", juce::SystemStats::getCpuModel().toRawUTF8(),
                 juce::SystemStats::getNumCpus(), juce::SystemStats::getOperatingSystemName().toRawUTF8());

    std::printf ("Default settings (bass + kick)\n");
    std::printf ("  rate     ch  block |  avg %% of RT | 99.9th pct %% | worst block %% | us / block\n");
    double worstAverage = 0.0;
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        const auto source = makeSource (sr, (int) (4.0 * sr));
        for (int ch : { 1, 2 })
            for (int bs : { 64, 256, 512, 1024 })
            {
                const auto r = measure (sr, ch, bs, seconds, Mode::defaults, source);
                worstAverage = std::max (worstAverage, r.averagePercent);
                std::printf ("  %5.1fk  %2d  %5d | %11.3f  | %11.2f  | %12.2f  | %9.1f\n", sr / 1000.0, ch, bs,
                             r.averagePercent, r.p999Percent, r.worstPercent, r.microsPerBlock);
            }
    }

    std::printf ("\nStereo, block 512: Vintage driven, 8x oversampling\n");
    for (double sr : { 48000.0, 96000.0 })
    {
        const auto source = makeSource (sr, (int) (4.0 * sr));
        const auto all = measure (sr, 2, 512, seconds, Mode::everything, source);
        std::printf ("  %5.1fk  %.3f %% of one core\n", sr / 1000.0, all.averagePercent);
        worstAverage = std::max (worstAverage, all.averagePercent);
    }

    std::printf ("\nHighest average load of all cases: %.3f %% of one core\n", worstAverage);
    return 0;
}
