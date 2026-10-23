#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

#include <memory>
#include <vector>

/** Arks Sonic 22 editor. Phase 2: functional layout (knobs, switches, meters, presets).
    The visual design is finished in Phase 8. Real-amp controls and simulator-only helpers are
    grouped and labelled separately. */
class Sonic22Editor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit Sonic22Editor (Sonic22Processor&);
    ~Sonic22Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    Knob& addKnob (const juce::String& paramID, const juce::String& text, const juce::String& tooltip);
    void timerCallback() override;
    void refreshPresetBox();

    Sonic22Processor& processor;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob* vintageVolume = nullptr; Knob* vintageTreble = nullptr; Knob* vintageBass = nullptr;
    Knob* burnGain1 = nullptr; Knob* burnGain2 = nullptr; Knob* burnTreble = nullptr;
    Knob* burnBass = nullptr; Knob* burnMid = nullptr; Knob* burnVolume = nullptr;
    Knob* reverb = nullptr; Knob* input = nullptr; Knob* output = nullptr;

    juce::ComboBox channelBox, oversamplingBox, presetBox;
    juce::ToggleButton fatButton { "Fat" }, bypassButton { "Bypass" };
    juce::Label channelLabel { {}, "Channel" }, osLabel { {}, "Oversampling" }, presetLabel { {}, "Preset" };

    std::unique_ptr<ComboBoxAttachment> channelAttachment, oversamplingAttachment;
    std::unique_ptr<ButtonAttachment>   fatAttachment, bypassAttachment;

    // Metering
    float inputMeter = 0.0f, outputMeter = 0.0f;
    int   overloadHold = 0;   // frames left to keep the overload LED lit

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sonic22Editor)
};
