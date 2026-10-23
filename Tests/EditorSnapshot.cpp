// Renders the plugin editor to a PNG (for checking the UI without a DAW).
// Usage: EditorSnapshot <output.png>

#include "PluginProcessor.h"

#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;

    const juce::File out (argc > 1 ? juce::String::fromUTF8 (argv[1])
                                   : juce::File::getCurrentWorkingDirectory().getChildFile ("editor.png").getFullPathName());

    Sonic22Processor proc;
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setVisible (true);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    const bool ok = stream.openedOk() && png.writeImageToStream (image, stream);
    std::printf ("%s %s (%d x %d)\n", ok ? "wrote" : "FAILED", out.getFullPathName().toRawUTF8(),
                 image.getWidth(), image.getHeight());
    return ok ? 0 : 1;
}
