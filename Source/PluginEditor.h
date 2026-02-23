#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

struct KnobWithLabel
{
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

struct ChoiceWithLabel
{
    juce::ComboBox combo;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

struct ToggleWithLabel
{
    juce::ToggleButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class WetStringReverbEditor : public juce::AudioProcessorEditor
{
public:
    explicit WetStringReverbEditor (WetStringReverbProcessor&);
    ~WetStringReverbEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WetStringReverbProcessor& processorRef;

    juce::LookAndFeel_V4 darkLookAndFeel;

    // Helper setup
    void setupKnob   (KnobWithLabel&,   const juce::String& paramId, const juce::String& labelText,
                      juce::Colour knobFill = juce::Colour (0xff7777bb));
    void setupChoice  (ChoiceWithLabel&, const juce::String& paramId, const juce::String& labelText);
    void setupToggle  (ToggleWithLabel&, const juce::String& paramId, const juce::String& labelText);

    // ---- MAIN ----
    KnobWithLabel dryWetKnob, preDelayKnob, earlyLevelKnob, lateLevelKnob,
                  roomSizeKnob, widthKnob;
    ChoiceWithLabel oversamplingChoice;

    // ---- REVERB ----
    KnobWithLabel lowRT60Knob, highRT60Knob, hfDampKnob, diffusionKnob, decayShapeKnob;

    // ---- SATURATION ----
    KnobWithLabel satAmountKnob, satDriveKnob, satToneKnob, satAsymmetryKnob;
    ChoiceWithLabel satTypeChoice;

    // ---- MODULATION ----
    KnobWithLabel modDepthKnob, modRateKnob;

    // ---- BYPASS TOGGLES ----
    ToggleWithLabel bypassEarly, bypassFDN, bypassDVN, bypassSaturation,
                    bypassToneFilter, bypassAttenFilter, bypassModulation;

    // ---- Buttons ----
    juce::TextButton saveButton  { "Save" };
    juce::TextButton loadButton  { "Load" };
    juce::TextButton imageButton { "Image" };

    // ---- Background image ----
    juce::Image backgroundImage;
    std::shared_ptr<juce::FileChooser> fileChooser;

    void savePreset();
    void loadPreset();
    void loadBackgroundImage();
    void restoreBackgroundImage();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WetStringReverbEditor)
};
