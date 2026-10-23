// Hosts the built VST3 the way a DAW does (JUCE's VST3 hosting) and checks:
//   - it scans and loads; name / vendor / latency
//   - mono, stereo and mono->stereo layouts (process runs, output finite and non-silent)
//   - Bypass makes a 110 Hz sine pass through untouched
//   - the host-side bypass parameter is the plugin's Bypass
//   - state round trip through the host; editor opens / closes
// Usage: Vst3HostTest "<path to Arks Sonic 22.vst3>"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <cstdio>

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    int failures = 0;
    void check (bool ok, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", ok ? "ok" : "NG", what.toRawUTF8());
        if (! ok) ++failures;
    }

    /** Max |out - in| for a 110 Hz sine over 50 blocks, in the given layout. */
    double maxDifference (juce::AudioPluginInstance& p, int inCh, int outCh)
    {
        p.prepareToPlay (sampleRate, blockSize);
        const int bufCh = juce::jmax (inCh, outCh);
        juce::MidiBuffer midi;
        double maxErr = 0.0;

        for (int b = 0; b < 50; ++b)
        {
            juce::AudioBuffer<float> buffer (bufCh, blockSize), ref (bufCh, blockSize);
            buffer.clear();
            ref.clear();
            for (int ch = 0; ch < inCh; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float s = (float) (0.5 * std::sin (juce::MathConstants<double>::twoPi * 110.0 * (b * blockSize + i) / sampleRate));
                    buffer.setSample (ch, i, s);
                    ref.setSample (ch, i, s);
                }

            p.processBlock (buffer, midi);

            for (int ch = 0; ch < outCh; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const int srcCh = ch < inCh ? ch : 0;
                    maxErr = juce::jmax (maxErr, (double) std::abs (buffer.getSample (ch, i) - ref.getSample (srcCh, i)));
                }
        }

        p.releaseResources();
        return maxErr;
    }

    /** Processes a 110 Hz sine and returns true if every output channel is finite and non-silent. */
    bool producesFiniteOutput (juce::AudioPluginInstance& p, int inCh, int outCh)
    {
        p.prepareToPlay (sampleRate, blockSize);
        const int bufCh = juce::jmax (inCh, outCh);
        juce::MidiBuffer midi;
        double peak[2] = { 0.0, 0.0 };
        bool finite = true;

        for (int b = 0; b < 50; ++b)
        {
            juce::AudioBuffer<float> buffer (bufCh, blockSize);
            buffer.clear();
            for (int ch = 0; ch < inCh; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (ch, i, (float) (0.3 * std::sin (juce::MathConstants<double>::twoPi * 110.0 * (b * blockSize + i) / sampleRate)));

            p.processBlock (buffer, midi);

            for (int ch = 0; ch < outCh; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float v = buffer.getSample (ch, i);
                    finite = finite && std::isfinite (v);
                    peak[juce::jmin (ch, 1)] = juce::jmax (peak[juce::jmin (ch, 1)], (double) std::abs (v));
                }
        }

        p.releaseResources();
        bool nonSilent = true;
        for (int ch = 0; ch < outCh; ++ch)
            nonSilent = nonSilent && peak[juce::jmin (ch, 1)] > 1.0e-4;
        return finite && nonSilent;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;

    if (argc < 2)
    {
        std::printf ("Usage: Vst3HostTest <path to .vst3>\n");
        return 2;
    }

    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);
    juce::VST3PluginFormat* vst3 = nullptr;
    for (auto* f : formats.getFormats())
        if (auto* v = dynamic_cast<juce::VST3PluginFormat*> (f))
            vst3 = v;

    juce::OwnedArray<juce::PluginDescription> found;
    vst3->findAllTypesForFile (found, juce::String::fromUTF8 (argv[1]));
    check (found.size() == 1, "scan finds one plugin");
    if (found.isEmpty())
        return 1;

    const auto& desc = *found[0];
    std::printf ("  plugin: %s / %s / %s\n", desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(),
                 desc.version.toRawUTF8());
    check (desc.name == "Arks Sonic 22", "name");
    check (desc.manufacturerName == "Arks Plugin", "vendor");

    juce::String error;
    auto plugin = formats.createPluginInstance (desc, sampleRate, blockSize, error);
    check (plugin != nullptr, "instance created " + error);
    if (plugin == nullptr)
        return 1;

    check (plugin->getLatencySamples() == 0, "latency 0");

    struct Layout { int in, out; const char* name; };
    for (auto l : { Layout { 2, 2, "stereo" }, Layout { 1, 1, "mono" }, Layout { 1, 2, "mono->stereo" } })
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add  (l.in  == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        layout.outputBuses.add (l.out == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        const bool ok = plugin->setBusesLayout (layout);
        check (ok, juce::String (l.name) + " layout accepted");
        if (! ok)
            continue;

        check (producesFiniteOutput (*plugin, l.in, l.out), juce::String (l.name) + ": processes to finite, non-silent output");
    }

    auto* bypass = plugin->getBypassParameter();
    check (bypass != nullptr && bypass->getName (32) == "Bypass", "host bypass parameter = Bypass");

    if (bypass != nullptr)
    {
        plugin->setBusesLayout ([] { juce::AudioProcessor::BusesLayout l;
            l.inputBuses.add (juce::AudioChannelSet::stereo());
            l.outputBuses.add (juce::AudioChannelSet::stereo()); return l; }());

        bypass->setValueNotifyingHost (1.0f);
        check (maxDifference (*plugin, 2, 2) == 0.0, "bypass: audio untouched");

        juce::MemoryBlock state;
        plugin->getStateInformation (state);
        bypass->setValueNotifyingHost (0.0f);
        plugin->setStateInformation (state.getData(), (int) state.getSize());
        check (bypass->getValue() > 0.5f, "state round trip through host");
        bypass->setValueNotifyingHost (0.0f);
    }

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorIfNeeded());
        check (editor != nullptr && editor->getWidth() > 0, "editor opens");
        editor.reset();
    }

    plugin.reset();
    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
