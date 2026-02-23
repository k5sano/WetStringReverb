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
    g.drawFittedText ("3-Layer Hybrid Reverb for Strings",
                      getLocalBounds().removeFromTop (90).removeFromBottom (30),
                      juce::Justification::centred, 1);

    g.setFont (12.0f);
    g.setColour (juce::Colour (0xff666688));
    g.drawFittedText ("OVN Early | FDN Core | DVN Tail",
                      getLocalBounds().reduced (20),
                      juce::Justification::centredBottom, 1);
}

void WetStringReverbEditor::resized()
{
    // プレースホルダー — 将来の GUI 拡張用
}
