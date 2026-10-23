#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "AmpEngine.h"
#include "Presets.h"

namespace ParamIDs
{
    // Shared
    inline constexpr auto channel      = "channel";       // 0 = Vintage, 1 = Burn
    inline constexpr auto reverb       = "reverb";
    inline constexpr auto fat          = "fat";           // Vintage voicing switch
    // Vintage channel
    inline constexpr auto vintageVolume = "vintageVolume";
    inline constexpr auto vintageTreble = "vintageTreble";
    inline constexpr auto vintageBass   = "vintageBass";
    // Burn channel
    inline constexpr auto burnGain1  = "burnGain1";
    inline constexpr auto burnGain2  = "burnGain2";
    inline constexpr auto burnTreble = "burnTreble";
    inline constexpr auto burnBass   = "burnBass";
    inline constexpr auto burnMid    = "burnMid";
    inline constexpr auto burnVolume = "burnVolume";
    // Simulator-only helpers (not on the real amp)
    inline constexpr auto input        = "input";
    inline constexpr auto output       = "output";
    inline constexpr auto oversampling = "oversampling";  // Off / 2x / 4x / 8x
    inline constexpr auto bypass       = "bypass";
}

/** Arks Sonic 22 - tube amplifier simulator (reference: Fender Super-Sonic 22 Combo). */
class Sonic22Processor final : public juce::AudioProcessor
{
public:
    Sonic22Processor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Factory presets as host programs.
    int getNumPrograms() override { return (int) Presets::all.size(); }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    /** Loads a factory preset (always applies, even if it is already the current one). */
    void applyPreset (int index);

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    AmpEngine::Meters& getMeters() { return engine.meters; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    AmpEngine::Params currentParams() const;

    AmpEngine engine;
    std::atomic<int> currentProgram { 0 };

    struct Raw
    {
        std::atomic<float>* channel = nullptr;
        std::atomic<float>* reverb = nullptr;
        std::atomic<float>* fat = nullptr;
        std::atomic<float>* vintageVolume = nullptr;
        std::atomic<float>* vintageTreble = nullptr;
        std::atomic<float>* vintageBass = nullptr;
        std::atomic<float>* burnGain1 = nullptr;
        std::atomic<float>* burnGain2 = nullptr;
        std::atomic<float>* burnTreble = nullptr;
        std::atomic<float>* burnBass = nullptr;
        std::atomic<float>* burnMid = nullptr;
        std::atomic<float>* burnVolume = nullptr;
        std::atomic<float>* input = nullptr;
        std::atomic<float>* output = nullptr;
        std::atomic<float>* oversampling = nullptr;
        std::atomic<float>* bypass = nullptr;
    } raw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sonic22Processor)
};
