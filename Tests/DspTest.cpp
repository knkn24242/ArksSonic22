// DSP checks on the real processor (Sonic22Processor::processBlock).
//
// Foundation checks (gain, bypass, mono->stereo, smoothing, safety) run on the BURN channel, which
// is still identity (Phase 4). Phase 3 adds Vintage-channel checks: the Fender Treble/Bass tone
// stack acts on the right bands, Volume drives V1-B into (even-harmonic) break-up, Fat boosts the
// low-mids, oversampling lowers aliasing, and the channel is stable at extreme settings.

#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace
{
    int failures = 0;
    constexpr double kPi = 3.14159265358979323846;

    void check (bool ok, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
        if (! ok) ++failures;
    }

    double db (double x) { return 20.0 * std::log10 (std::max (x, 1.0e-20)); }

    using Signal = std::vector<std::vector<float>>;

    struct Settings
    {
        double sampleRate = 48000.0;
        int blockSize = 512;
        int channel = 0;              // 0 = Vintage, 1 = Burn
        float treble = 50, bass = 50, volume = 50;
        float burnG1 = 50, burnG2 = 50, burnTre = 50, burnBas = 50, burnMid = 50, burnVol = 50;
        bool fat = false;
        int os = 1;                   // oversampling index: 0=Off,1=2x,2=4x,3=8x
        float input = 0.0f, output = 0.0f;
        bool bypass = false;
        int outChannels = -1;
    };

    void setParam (Sonic22Processor& p, const char* id, float v)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (v));
    }

    Signal run (const Signal& in, const Settings& s,
                std::function<void (Sonic22Processor&, int)> perBlock = {})
    {
        const int numIn = (int) in.size();
        const int numOut = s.outChannels < 0 ? numIn : s.outChannels;
        const int n = (int) in[0].size();

        Sonic22Processor proc;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add  (numIn  == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        layout.outputBuses.add (numOut == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        proc.setBusesLayout (layout);

        setParam (proc, ParamIDs::channel, (float) s.channel);
        setParam (proc, ParamIDs::vintageTreble, s.treble);
        setParam (proc, ParamIDs::vintageBass, s.bass);
        setParam (proc, ParamIDs::vintageVolume, s.volume);
        setParam (proc, ParamIDs::burnGain1, s.burnG1);
        setParam (proc, ParamIDs::burnGain2, s.burnG2);
        setParam (proc, ParamIDs::burnTreble, s.burnTre);
        setParam (proc, ParamIDs::burnBass, s.burnBas);
        setParam (proc, ParamIDs::burnMid, s.burnMid);
        setParam (proc, ParamIDs::burnVolume, s.burnVol);
        setParam (proc, ParamIDs::fat, s.fat ? 1.0f : 0.0f);
        setParam (proc, ParamIDs::oversampling, (float) s.os);
        setParam (proc, ParamIDs::input, s.input);
        setParam (proc, ParamIDs::output, s.output);
        setParam (proc, ParamIDs::bypass, s.bypass ? 1.0f : 0.0f);

        proc.setRateAndBufferSizeDetails (s.sampleRate, s.blockSize);
        proc.prepareToPlay (s.sampleRate, s.blockSize);

        const int bufCh = juce::jmax (numIn, numOut);
        Signal out (bufCh, std::vector<float> ((size_t) n, 0.0f));
        juce::AudioBuffer<float> buffer (bufCh, s.blockSize);
        juce::MidiBuffer midi;

        for (int start = 0; start < n; start += s.blockSize)
        {
            const int len = std::min (s.blockSize, n - start);
            if (perBlock) perBlock (proc, start);
            buffer.setSize (bufCh, len, false, false, true);
            buffer.clear();
            for (int ch = 0; ch < numIn; ++ch)
                buffer.copyFrom (ch, 0, in[(size_t) ch].data() + start, len);
            proc.processBlock (buffer, midi);
            for (int ch = 0; ch < bufCh; ++ch)
                std::copy (buffer.getReadPointer (ch), buffer.getReadPointer (ch) + len, out[(size_t) ch].data() + start);
        }
        return out;
    }

    std::vector<float> sine (double freq, double amp, double sr, int n)
    {
        std::vector<float> x ((size_t) n);
        for (int i = 0; i < n; ++i)
            x[(size_t) i] = (float) (amp * std::sin (juce::MathConstants<double>::twoPi * freq * i / sr));
        return x;
    }

    /** Hann-windowed single-frequency amplitude estimate over [from, end). */
    double ampAt (const std::vector<float>& x, double hz, double sr, size_t from = 0)
    {
        const size_t n = x.size() - from;
        double re = 0.0, im = 0.0, wsum = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * (double) i / (double) (n - 1));
            const double s = (double) x[from + i] * w;
            const double ph = 2.0 * kPi * hz * (double) i / sr;
            re += s * std::cos (ph);
            im -= s * std::sin (ph);
            wsum += w;
        }
        return 2.0 * std::sqrt (re * re + im * im) / wsum;
    }

    bool allFinite (const Signal& s)
    {
        for (auto& ch : s) for (auto v : ch) if (! std::isfinite (v)) return false;
        return true;
    }

    double maxStep (const std::vector<float>& x, size_t from = 1)
    {
        double m = 0.0;
        for (size_t i = std::max<size_t> (1, from); i < x.size(); ++i)
            m = std::max (m, (double) std::abs (x[i] - x[i - 1]));
        return m;
    }

    double peakOf (const std::vector<float>& x, size_t from = 0)
    {
        double m = 0.0;
        for (size_t i = from; i < x.size(); ++i) m = std::max (m, (double) std::abs (x[i]));
        return m;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    const double sr = 48000.0;
    const int n = (int) (2.0 * sr);
    const size_t skip = (size_t) (0.3 * sr);   // skip start-up transient / ramps

    //==============================================================================
    std::printf ("Foundation: silence, bypass, input gain, mono->stereo\n");
    {
        Signal silence { std::vector<float> ((size_t) n, 0.0f), std::vector<float> ((size_t) n, 0.0f) };
        auto out = run (silence, {});
        check (peakOf (out[0]) == 0.0 && peakOf (out[1]) == 0.0, "silence in -> exact silence out");

        Signal c { sine (120.0, 0.3, sr, n), sine (90.0, 0.3, sr, n) };
        Settings byp; byp.bypass = true; byp.input = 9.0f; byp.output = -5.0f;
        check (run (c, byp) == c, "bypass with gains -> bit-identical");

        // Input gain raises the output level (the amp is nonlinear, so not an exact ratio).
        Signal s { sine (120.0, 0.2, sr, n) };
        Settings lo; lo.channel = 0; lo.volume = 40; lo.input = -6.0f;
        Settings hi = lo; hi.input = 6.0f;
        const double loPk = peakOf (run (s, lo)[0], skip), hiPk = peakOf (run (s, hi)[0], skip);
        check (hiPk > loPk * 1.3, "more Input drive raises the output level");

        Settings ms; ms.channel = 0; ms.volume = 60; ms.outChannels = 2;
        auto mo = run (s, ms);
        bool equalAndLive = peakOf (mo[0], skip) > 1.0e-3;
        for (size_t i = skip; i < s[0].size(); ++i) equalAndLive = equalAndLive && mo[0][i] == mo[1][i];
        check (equalAndLive, "mono in -> stereo out: both channels identical and non-silent");
    }

    //==============================================================================
    std::printf ("\nVintage tone stack: Treble acts on highs, Bass acts on lows\n");
    {
        auto toneLevel = [&] (float treble, float bass, double hz)
        {
            Settings v; v.channel = 0; v.treble = treble; v.bass = bass; v.volume = 40; v.input = -6.0f;
            Signal in { sine (hz, 0.2, sr, n) };
            return db (ampAt (run (in, v)[0], hz, sr, skip) / ampAt (in[0], hz, sr, skip));
        };
        const double t3kLo = toneLevel (10, 50, 3000.0);
        const double t3kHi = toneLevel (90, 50, 3000.0);
        std::printf ("  3 kHz: Treble 10 -> %.1f dB, Treble 90 -> %.1f dB\n", t3kLo, t3kHi);
        check (t3kHi - t3kLo > 8.0, "Treble raises 3 kHz by > 8 dB from min to max");

        const double b100Lo = toneLevel (50, 10, 100.0);
        const double b100Hi = toneLevel (50, 90, 100.0);
        std::printf ("  100 Hz: Bass 10 -> %.1f dB, Bass 90 -> %.1f dB\n", b100Lo, b100Hi);
        check (b100Hi - b100Lo > 6.0, "Bass raises 100 Hz by > 6 dB from min to max");
    }

    //==============================================================================
    std::printf ("\nVintage Volume drives V1-B into break-up (even-harmonic warmth)\n");
    {
        auto harmonics = [&] (float volume, double f0, double& h2, double& level)
        {
            Settings v; v.channel = 0; v.volume = volume; v.input = -3.0f;
            Signal in { sine (f0, 0.3, sr, n) };
            auto out = run (in, v);
            const double fund = ampAt (out[0], f0, sr, skip);
            h2 = ampAt (out[0], 2 * f0, sr, skip);
            const double h3 = ampAt (out[0], 3 * f0, sr, skip);
            level = db (fund);
            return (h2 + h3) / std::max (fund, 1e-12);
        };
        double h2lo, h2hi, levLo, levHi;
        const double thdLo = harmonics (25, 220.0, h2lo, levLo);
        const double thdHi = harmonics (95, 220.0, h2hi, levHi);
        std::printf ("  220 Hz: Vol 25 -> 2nd+3rd %.1f dB, fund %.1f dB | Vol 95 -> 2nd+3rd %.1f dB, fund %.1f dB\n",
                     db (thdLo), levLo, db (thdHi), levHi);
        check (thdHi > thdLo * 2.0, "higher Volume generates more harmonic content");
        // h2hi is the 2nd-harmonic amplitude; levHi is the fundamental in dB.
        check (db (h2hi) - levHi > -30.0, "2nd harmonic clearly present re fundamental (single-stage warmth)");
    }

    //==============================================================================
    std::printf ("\nVintage Fat switch boosts the low-mids\n");
    {
        auto lowMid = [&] (bool fat, double hz)
        {
            Settings v; v.channel = 0; v.fat = fat; v.volume = 40; v.input = -6.0f;
            Signal in { sine (hz, 0.2, sr, n) };
            return db (ampAt (run (in, v)[0], hz, sr, skip));
        };
        const double off = lowMid (false, 180.0);
        const double on  = lowMid (true, 180.0);
        std::printf ("  180 Hz: Fat off %.1f dB, Fat on %.1f dB (%+.1f dB)\n", off, on, on - off);
        check (on - off > 1.5, "Fat raises the 180 Hz low-mid band");
    }

    //==============================================================================
    std::printf ("\nBurn channel: cascaded gain structure and TMB tone\n");
    {
        auto burnThd = [&] (float g1, float g2, double f0)
        {
            Settings v; v.channel = 1; v.burnG1 = g1; v.burnG2 = g2; v.burnVol = 50; v.input = -6.0f;
            Signal in { sine (f0, 0.2, sr, n) };
            auto out = run (in, v);
            const double fund = ampAt (out[0], f0, sr, skip);
            double harm = 0.0;
            for (int k = 2; k <= 6; ++k) { const double h = ampAt (out[0], k * f0, sr, skip); harm += h * h; }
            return std::sqrt (harm) / std::max (fund, 1e-12);
        };
        const double low  = burnThd (20, 20, 220.0);
        const double high = burnThd (90, 85, 220.0);
        std::printf ("  220 Hz THD: Gain low %.1f %%, Gain high %.1f %%\n", 100.0 * low, 100.0 * high);
        check (high > low * 2.0 && high > 0.1, "raising Gain 1 / Gain 2 adds heavy harmonic distortion");

        // Burn is a higher-gain channel than Vintage at the same input / controls at noon.
        auto chThd = [&] (int channel)
        {
            Settings v; v.channel = channel; v.volume = 50; v.burnG1 = 50; v.burnG2 = 50; v.input = -6.0f;
            Signal in { sine (220.0, 0.2, sr, n) };
            auto out = run (in, v);
            const double fund = ampAt (out[0], 220.0, sr, skip);
            double harm = 0.0;
            for (int k = 2; k <= 6; ++k) { const double h = ampAt (out[0], k * 220.0, sr, skip); harm += h * h; }
            return std::sqrt (harm) / std::max (fund, 1e-12);
        };
        const double vinThd = chThd (0), burThd = chThd (1);
        std::printf ("  noon THD: Vintage %.1f %%, Burn %.1f %%\n", 100.0 * vinThd, 100.0 * burThd);
        check (burThd > vinThd, "Burn is more saturated than Vintage at the same settings");

        // TMB tone: Treble (3 kHz), Middle (750 Hz), Bass (100 Hz) each act on their band.
        auto band = [&] (const char* which, float lo, float hi, double hz)
        {
            auto level = [&] (float v)
            {
                Settings s; s.channel = 1; s.burnG1 = 30; s.burnG2 = 30; s.burnVol = 50; s.input = -10.0f;
                s.burnTre = 50; s.burnMid = 50; s.burnBas = 50;
                if (which[0] == 'T') s.burnTre = v; else if (which[0] == 'M') s.burnMid = v; else s.burnBas = v;
                Signal in { sine (hz, 0.15, sr, n) };
                return db (ampAt (run (in, s)[0], hz, sr, skip) / ampAt (in[0], hz, sr, skip));
            };
            const double d = level (hi) - level (lo);
            std::printf ("  %-6s %.0f Hz: %.0f -> %.0f moves the band by %+.1f dB\n", which, hz, lo, hi, d);
            return d;
        };
        check (band ("Treble", 10, 90, 3000.0) > 6.0, "Burn Treble raises the high band");
        check (band ("Middle", 10, 90, 750.0) > 3.0, "Burn Middle raises the mid band");
        check (band ("Bass", 10, 90, 100.0) > 4.0, "Burn Bass raises the low band");

        // Stability at full gain.
        Settings ex; ex.channel = 1; ex.burnG1 = 100; ex.burnG2 = 100; ex.burnVol = 100; ex.input = 18.0f;
        Signal hot { sine (110.0, 0.8, sr, n) };
        auto exOut = run (hot, ex);
        check (allFinite (exOut) && peakOf (exOut[0]) < 10.0, "Burn at full gain: finite and bounded");
    }

    //==============================================================================
    std::printf ("\nOversampling lowers aliasing (10 kHz sine; 3rd harmonic at 30 kHz folds to 18 kHz)\n");
    {
        auto alias = [&] (int os)
        {
            Settings v; v.channel = 0; v.volume = 95; v.input = 0.0f; v.os = os;
            Signal in { sine (10000.0, 0.5, sr, n) };
            return db (ampAt (run (in, v)[0], 18000.0, sr, skip) / 0.5);
        };
        const double off = alias (0);   // Off
        const double x8  = alias (3);   // 8x
        std::printf ("  18 kHz alias: OS off %.1f dB, OS 8x %.1f dB (%+.1f dB)\n", off, x8, x8 - off);
        check (off - x8 > 10.0, "8x oversampling lowers the 18 kHz alias by > 10 dB vs off");
    }

    //==============================================================================
    std::printf ("\nStability / safety\n");
    {
        // Click-free bypass toggle on the (now colouring) Vintage channel.
        Signal in { sine (120.0, 0.5, sr, n * 2) };
        Settings v; v.channel = 0; v.volume = 60;
        const double steady = maxStep (run (in, v)[0], skip);
        auto toggled = run (in, v, [&] (Sonic22Processor& p, int idx)
        {
            setParam (p, ParamIDs::bypass, ((idx / (int) (0.25 * sr)) % 2) ? 1.0f : 0.0f);
        });
        const double step = maxStep (toggled[0], (size_t) (0.1 * sr));
        std::printf ("  steady max step %.5f, bypass toggled every 250 ms %.5f\n", steady, step);
        check (step <= steady * 1.1 + 1.0e-5, "bypass on/off: no click (step not larger than the signal's own)");

        // Extreme settings: hot input, Volume 100, tone extremes -> finite and bounded.
        juce::Random rng (9);
        Signal hot { sine (82.4, 0.9, sr, n), sine (110.0, 0.9, sr, n) };
        Settings ex; ex.channel = 0; ex.volume = 100; ex.treble = 100; ex.bass = 100; ex.fat = true; ex.input = 24.0f;
        auto exOut = run (hot, ex);
        std::printf ("  extreme settings output peak %.2f\n", peakOf (exOut[0]));
        check (allFinite (exOut), "extreme settings: output finite");
        check (peakOf (exOut[0]) < 10.0, "extreme settings: output bounded");

        // Long run with random automation.
        const int big = (int) (120.0 * sr);
        auto base = sine (55.0, 0.4, sr, (int) (4 * sr));
        Signal lin (2, std::vector<float> ((size_t) big));
        for (int i = 0; i < big; ++i)
        {
            const float val = base[(size_t) (i % (int) (4 * sr))] + 0.01f * (rng.nextFloat() * 2.0f - 1.0f);
            lin[0][(size_t) i] = val; lin[1][(size_t) i] = 0.9f * val;
        }
        Settings lv; lv.channel = 0;
        auto lout = run (lin, lv, [&] (Sonic22Processor& p, int idx)
        {
            if (idx % (int) sr < 512)
            {
                setParam (p, ParamIDs::vintageVolume, 100.0f * rng.nextFloat());
                setParam (p, ParamIDs::vintageTreble, 100.0f * rng.nextFloat());
                setParam (p, ParamIDs::vintageBass, 100.0f * rng.nextFloat());
            }
        });
        check (allFinite (lout) && peakOf (lout[0]) < 10.0, "2 min run with automation: finite and bounded");
    }

    //==============================================================================
    std::printf ("\nMeters and presets\n");
    {
        Signal in { sine (120.0, 0.5, sr, (int) sr) };
        float mIn = 0.0f, mOut = 0.0f;
        Settings v; v.channel = 0; v.volume = 50;
        run (in, v, [&] (Sonic22Processor& p, int)
        {
            auto& m = p.getMeters();
            mIn = juce::jmax (mIn, AmpEngine::Meters::take (m.input));
            mOut = juce::jmax (mOut, AmpEngine::Meters::take (m.output));
        });
        std::printf ("  input %.2f dBFS, output %.2f dBFS\n", db (mIn), db (mOut));
        check (std::abs (db (mIn) - db (0.5)) < 0.2, "input meter reads the real input peak");
        check (mOut > 0.001f, "output meter follows the processed signal");

        bool allOk = true;
        for (int i = 0; i < (int) Presets::all.size(); ++i)
        {
            const auto& pr = Presets::all[(size_t) i];
            Sonic22Processor probe;
            probe.applyPreset (i);
            allOk = allOk && (int) probe.apvts.getRawParameterValue (ParamIDs::channel)->load() == pr.channel
                         && std::abs (probe.apvts.getRawParameterValue (ParamIDs::vintageVolume)->load() - pr.vintageVolume) < 0.05f
                         && probe.getProgramName (i) == pr.name;
        }
        check (allOk, "every preset loads its values and is reported to the host");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
