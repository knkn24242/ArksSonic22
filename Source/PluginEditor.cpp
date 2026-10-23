#include "PluginEditor.h"

namespace
{
    constexpr int knobW = 92, knobH = 108, pad = 10;
}

Sonic22Editor::Knob& Sonic22Editor::addKnob (const juce::String& paramID, const juce::String& text,
                                             const juce::String& tooltip)
{
    auto k = std::make_unique<Knob>();

    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k->slider.setTooltip (tooltip);

    // Double-click returns the control to its parameter default.
    if (auto* p = processor.apvts.getParameter (paramID))
        k->slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::Font (13.0f, juce::Font::bold));

    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<SliderAttachment> (processor.apvts, paramID, k->slider);

    knobs.push_back (std::move (k));
    return *knobs.back();
}

Sonic22Editor::Sonic22Editor (Sonic22Processor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    vintageVolume = &addKnob (ParamIDs::vintageVolume, "Volume",  "Vintage channel volume");
    vintageTreble = &addKnob (ParamIDs::vintageTreble, "Treble",  "Vintage channel treble");
    vintageBass   = &addKnob (ParamIDs::vintageBass,   "Bass",    "Vintage channel bass");

    burnGain1  = &addKnob (ParamIDs::burnGain1,  "Gain 1", "Burn channel first gain stage");
    burnGain2  = &addKnob (ParamIDs::burnGain2,  "Gain 2", "Burn channel second gain stage");
    burnTreble = &addKnob (ParamIDs::burnTreble, "Treble", "Burn channel treble");
    burnBass   = &addKnob (ParamIDs::burnBass,   "Bass",   "Burn channel bass");
    burnMid    = &addKnob (ParamIDs::burnMid,    "Middle", "Burn channel middle");
    burnVolume = &addKnob (ParamIDs::burnVolume, "Volume", "Burn channel volume");

    reverb = &addKnob (ParamIDs::reverb, "Reverb", "Shared spring reverb amount");
    input  = &addKnob (ParamIDs::input,  "Input",  "Input level (simulator helper, not on the real amp)");
    output = &addKnob (ParamIDs::output, "Output", "Output level (simulator helper, not on the real amp)");

    // Channel selector
    channelBox.addItemList ({ "Vintage", "Burn" }, 1);
    addAndMakeVisible (channelBox);
    channelAttachment = std::make_unique<ComboBoxAttachment> (processor.apvts, ParamIDs::channel, channelBox);
    channelLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (channelLabel);

    // Fat / Bypass
    addAndMakeVisible (fatButton);
    fatAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ParamIDs::fat, fatButton);
    fatButton.setTooltip ("Vintage voicing switch (fatter mids)");

    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ParamIDs::bypass, bypassButton);
    bypassButton.setTooltip ("Bypass the amp (click-free)");

    // Oversampling (simulator helper)
    oversamplingBox.addItemList ({ "Off", "2x", "4x", "8x" }, 1);
    addAndMakeVisible (oversamplingBox);
    oversamplingAttachment = std::make_unique<ComboBoxAttachment> (processor.apvts, ParamIDs::oversampling, oversamplingBox);
    osLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (osLabel);

    // Presets
    for (int i = 0; i < (int) Presets::all.size(); ++i)
        presetBox.addItem (Presets::all[(size_t) i].name, i + 1);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0)
            processor.applyPreset (idx);
    };
    addAndMakeVisible (presetBox);
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetLabel);
    refreshPresetBox();

    setSize (760, 500);
    startTimerHz (30);
}

Sonic22Editor::~Sonic22Editor()
{
    stopTimer();
}

void Sonic22Editor::refreshPresetBox()
{
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
}

void Sonic22Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff202428));

    auto drawGroup = [&g] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (juce::Colour (0xff2c3137));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (juce::Colour (0xff6f7b86));
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText (title, r.removeFromTop (18).reduced (8, 2), juce::Justification::centredLeft);
    };

    drawGroup ({ pad, 70, 360, 160 }, "VINTAGE");
    drawGroup ({ pad, 240, 740, 160 }, "BURN");
    drawGroup ({ pad, 410, 740, 80 }, "MASTER / SIMULATOR");

    // Meters (top right): input and output peak, plus overload LED.
    const int mx = 400, my = 12, mw = 346, barH = 16;
    g.setColour (juce::Colour (0xff6f7b86));
    g.setFont (juce::Font (11.0f));
    g.drawText ("IN",  mx - 28, my,          26, barH, juce::Justification::centredRight);
    g.drawText ("OUT", mx - 28, my + barH + 6, 26, barH, juce::Justification::centredRight);

    auto drawBar = [&] (int y, float level)
    {
        juce::Rectangle<int> r (mx, y, mw, barH);
        g.setColour (juce::Colour (0xff15181b));
        g.fillRect (r);
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto fill = r.withWidth (juce::roundToInt (mw * frac));
        g.setColour (level >= 1.0f ? juce::Colours::red
                                   : (db > -6.0f ? juce::Colours::orange : juce::Colours::limegreen));
        g.fillRect (fill);
    };
    drawBar (my, inputMeter);
    drawBar (my + barH + 6, outputMeter);

    g.setColour (overloadHold > 0 ? juce::Colours::red : juce::Colour (0xff3a3f45));
    g.fillEllipse ((float) (mx + mw + 8), (float) my, 14.0f, 14.0f);
    g.setColour (juce::Colour (0xff6f7b86));
    g.setFont (juce::Font (10.0f));
    g.drawText ("OL", mx + mw + 4, my + 16, 22, 12, juce::Justification::centred);
}

void Sonic22Editor::resized()
{
    // Top bar: preset, channel, oversampling, fat, bypass
    presetLabel.setBounds (pad, 8, 50, 22);
    presetBox.setBounds (pad + 52, 8, 150, 24);
    channelLabel.setBounds (pad + 212, 8, 56, 22);
    channelBox.setBounds (pad + 268, 8, 96, 24);

    auto placeKnob = [] (Knob* k, int x, int y)
    {
        k->label.setBounds (x, y, knobW, 18);
        k->slider.setBounds (x, y + 18, knobW, knobH - 18);
    };

    // Vintage group (y 70-230)
    int x = pad + 8, y = 94;
    placeKnob (vintageVolume, x, y);            x += knobW;
    placeKnob (vintageTreble, x, y);            x += knobW;
    placeKnob (vintageBass,   x, y);            x += knobW;
    fatButton.setBounds (x + 6, y + 36, 60, 24);

    // Burn group (y 240-400)
    x = pad + 8; y = 264;
    for (Knob* k : { burnGain1, burnGain2, burnTreble, burnBass, burnMid, burnVolume })
    {
        placeKnob (k, x, y);
        x += knobW;
    }

    // Master / simulator group (y 410-490)
    x = pad + 8; y = 428;
    placeKnob (reverb, x, y - 6);               x += knobW;
    placeKnob (input,  x, y - 6);               x += knobW;
    placeKnob (output, x, y - 6);               x += knobW + 10;
    osLabel.setBounds (x, y + 6, 84, 22);
    oversamplingBox.setBounds (x + 86, y + 6, 80, 24);
    bypassButton.setBounds (x + 180, y + 6, 90, 24);
}

void Sonic22Editor::timerCallback()
{
    auto& m = processor.getMeters();
    inputMeter  = juce::jmax (inputMeter  * 0.85f, AmpEngine::Meters::take (m.input));
    outputMeter = juce::jmax (outputMeter * 0.85f, AmpEngine::Meters::take (m.output));

    if (m.overload.exchange (false))
        overloadHold = 30;         // ~1 s at 30 Hz
    else if (overloadHold > 0)
        --overloadHold;

    refreshPresetBox();
    repaint();
}
