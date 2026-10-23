#include "PluginProcessor.h"
#include "PluginEditor.h"

Sonic22Processor::Sonic22Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ArksSonic22", createLayout())
{
    raw.channel       = apvts.getRawParameterValue (ParamIDs::channel);
    raw.reverb        = apvts.getRawParameterValue (ParamIDs::reverb);
    raw.fat           = apvts.getRawParameterValue (ParamIDs::fat);
    raw.vintageVolume = apvts.getRawParameterValue (ParamIDs::vintageVolume);
    raw.vintageTreble = apvts.getRawParameterValue (ParamIDs::vintageTreble);
    raw.vintageBass   = apvts.getRawParameterValue (ParamIDs::vintageBass);
    raw.burnGain1     = apvts.getRawParameterValue (ParamIDs::burnGain1);
    raw.burnGain2     = apvts.getRawParameterValue (ParamIDs::burnGain2);
    raw.burnTreble    = apvts.getRawParameterValue (ParamIDs::burnTreble);
    raw.burnBass      = apvts.getRawParameterValue (ParamIDs::burnBass);
    raw.burnMid       = apvts.getRawParameterValue (ParamIDs::burnMid);
    raw.burnVolume    = apvts.getRawParameterValue (ParamIDs::burnVolume);
    raw.input         = apvts.getRawParameterValue (ParamIDs::input);
    raw.output        = apvts.getRawParameterValue (ParamIDs::output);
    raw.oversampling  = apvts.getRawParameterValue (ParamIDs::oversampling);
    raw.bypass        = apvts.getRawParameterValue (ParamIDs::bypass);
}

juce::AudioProcessorValueTreeState::ParameterLayout Sonic22Processor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = AudioParameterFloatAttributes().withLabel ("%")
                      .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });
    auto db = AudioParameterFloatAttributes().withLabel ("dB")
                  .withStringFromValueFunction ([] (float v, int)
                  {
                      return (v > 0.04f ? "+" : "") + String (std::abs (v) < 0.05f ? 0.0f : v, 1) + " dB";
                  });

    auto knob = [&percent] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                      NormalisableRange<float> (0.0f, 100.0f, 0.1f), def, percent);
    };

    const auto& d = Presets::all[0];   // Vintage Clean: default control positions

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::channel, 1 }, "Channel",
                                                        StringArray { "Vintage", "Burn" }, d.channel));

    // Vintage
    layout.add (knob (ParamIDs::vintageVolume, "Vintage Volume", d.vintageVolume));
    layout.add (knob (ParamIDs::vintageTreble, "Vintage Treble", d.vintageTreble));
    layout.add (knob (ParamIDs::vintageBass,   "Vintage Bass",   d.vintageBass));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::fat, 1 }, "Fat", d.fat != 0));

    // Burn
    layout.add (knob (ParamIDs::burnGain1,  "Burn Gain 1", d.burnGain1));
    layout.add (knob (ParamIDs::burnGain2,  "Burn Gain 2", d.burnGain2));
    layout.add (knob (ParamIDs::burnTreble, "Burn Treble", d.burnTreble));
    layout.add (knob (ParamIDs::burnBass,   "Burn Bass",   d.burnBass));
    layout.add (knob (ParamIDs::burnMid,    "Burn Middle", d.burnMid));
    layout.add (knob (ParamIDs::burnVolume, "Burn Volume", d.burnVolume));

    // Shared
    layout.add (knob (ParamIDs::reverb, "Reverb", d.reverb));

    // Simulator-only helpers
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::input, 1 }, "Input",
                                                       NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, db));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, db));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::oversampling, 1 }, "Oversampling",
                                                        StringArray { "Off", "2x", "4x", "8x" }, 2));   // default 4x
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));
    return layout;
}

void Sonic22Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (0);
}

bool Sonic22Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    // mono -> mono, stereo -> stereo, and mono -> stereo (the amp is mono; it fans out to the output).
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

AmpEngine::Params Sonic22Processor::currentParams() const
{
    AmpEngine::Params p;
    p.channel       = raw.channel->load() < 0.5f ? AmpEngine::Channel::vintage : AmpEngine::Channel::burn;
    p.vintageVolume = raw.vintageVolume->load() * 0.01f;
    p.vintageTreble = raw.vintageTreble->load() * 0.01f;
    p.vintageBass   = raw.vintageBass->load() * 0.01f;
    p.fat           = raw.fat->load() > 0.5f;
    p.burnGain1     = raw.burnGain1->load() * 0.01f;
    p.burnGain2     = raw.burnGain2->load() * 0.01f;
    p.burnTreble    = raw.burnTreble->load() * 0.01f;
    p.burnBass      = raw.burnBass->load() * 0.01f;
    p.burnMid       = raw.burnMid->load() * 0.01f;
    p.burnVolume    = raw.burnVolume->load() * 0.01f;
    p.reverb        = raw.reverb->load() * 0.01f;
    p.inputDb       = raw.input->load();
    p.outputDb      = raw.output->load();
    p.bypass        = raw.bypass->load() > 0.5f;
    const int os[]  = { 1, 2, 4, 8 };
    p.osFactor      = os[juce::jlimit (0, 3, (int) raw.oversampling->load())];
    return p;
}

void Sonic22Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();

    // Clear any output channel that carries no input.
    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Mono in -> stereo out: fan the mono signal out so every output channel carries the amp.
    for (int i = juce::jmax (1, totalIn); i < totalOut; ++i)
        buffer.copyFrom (i, 0, buffer, 0, 0, buffer.getNumSamples());

    engine.process (buffer, currentParams());
}

juce::AudioProcessorParameter* Sonic22Processor::getBypassParameter() const
{
    return apvts.getParameter (ParamIDs::bypass);
}

//==============================================================================
const juce::String Sonic22Processor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) Presets::all.size()) ? Presets::all[(size_t) index].name : "";
}

void Sonic22Processor::setCurrentProgram (int index)
{
    // Hosts may re-send the current program while restoring a session; only a real change loads it.
    if (index != currentProgram.load())
        applyPreset (index);
}

void Sonic22Processor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) Presets::all.size()))
        return;

    const auto& pr = Presets::all[(size_t) index];
    auto set = [this] (const char* id, float value)
    {
        auto* param = apvts.getParameter (id);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    };

    set (ParamIDs::channel,       (float) pr.channel);
    set (ParamIDs::vintageVolume, pr.vintageVolume);
    set (ParamIDs::vintageTreble, pr.vintageTreble);
    set (ParamIDs::vintageBass,   pr.vintageBass);
    set (ParamIDs::fat,           (float) pr.fat);
    set (ParamIDs::burnGain1,     pr.burnGain1);
    set (ParamIDs::burnGain2,     pr.burnGain2);
    set (ParamIDs::burnTreble,    pr.burnTreble);
    set (ParamIDs::burnBass,      pr.burnBass);
    set (ParamIDs::burnMid,       pr.burnMid);
    set (ParamIDs::burnVolume,    pr.burnVolume);
    set (ParamIDs::reverb,        pr.reverb);
    currentProgram = index;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

//==============================================================================
void Sonic22Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Sonic22Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, (int) Presets::all.size() - 1, (int) tree.getProperty ("program", 0));
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* Sonic22Processor::createEditor()
{
    return new Sonic22Editor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Sonic22Processor();
}
