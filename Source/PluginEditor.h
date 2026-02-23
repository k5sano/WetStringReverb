#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class WetStringReverbEditor : public juce::AudioProcessorEditor
{
public:
    explicit WetStringReverbEditor (WetStringReverbProcessor&);
    ~WetStringReverbEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WetStringReverbProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WetStringReverbEditor)
};
