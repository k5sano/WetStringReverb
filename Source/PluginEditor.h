#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class VelvetUnderDronEditor : public juce::AudioProcessorEditor
{
public:
    explicit VelvetUnderDronEditor (VelvetUnderDronProcessor&);
    ~VelvetUnderDronEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    VelvetUnderDronProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelvetUnderDronEditor)
};
