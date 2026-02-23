#include "PluginEditor.h"

WetStringReverbEditor::WetStringReverbEditor (WetStringReverbProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (600, 400);
}

void WetStringReverbEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));

    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawFittedText ("WetStringReverb", getLocalBounds().removeFromTop (60),
                      juce::Justification::centred, 1);

    g.setFont (14.0f);
    g.setColour (juce::Colour (0xffaaaacc));
    g.drawFittedText ("Clouds / Dattorro Plate Reverb",
                      getLocalBounds().removeFromTop (90).removeFromBottom (30),
                      juce::Justification::centred, 1);
}

void WetStringReverbEditor::resized()
{
}
