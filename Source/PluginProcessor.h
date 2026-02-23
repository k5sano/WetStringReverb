#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "DSP/EarlyReflections.h"
#include "DSP/FDNReverb.h"
#include "DSP/DarkVelvetNoise.h"
#include "DSP/OversamplingManager.h"
#include "DSP/ReverbMixer.h"

class WetStringReverbProcessor : public juce::AudioProcessor
{
public:
    WetStringReverbProcessor();
    ~WetStringReverbProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "WetStringReverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    // Parameter atomic pointers
    std::atomic<float>* dryWetParam       = nullptr;
    std::atomic<float>* preDelayParam     = nullptr;
    std::atomic<float>* earlyLevelParam   = nullptr;
    std::atomic<float>* lateLevelParam    = nullptr;
    std::atomic<float>* roomSizeParam     = nullptr;
    std::atomic<float>* stereoWidthParam  = nullptr;
    std::atomic<float>* oversamplingParam = nullptr;

    std::atomic<float>* lowRT60Param      = nullptr;
    std::atomic<float>* highRT60Param     = nullptr;
    std::atomic<float>* hfDampingParam    = nullptr;
    std::atomic<float>* diffusionParam    = nullptr;
    std::atomic<float>* decayShapeParam   = nullptr;

    std::atomic<float>* satAmountParam    = nullptr;
    std::atomic<float>* satDriveParam     = nullptr;
    std::atomic<float>* satTypeParam      = nullptr;
    std::atomic<float>* satToneParam      = nullptr;
    std::atomic<float>* satAsymmetryParam = nullptr;

    std::atomic<float>* modDepthParam     = nullptr;
    std::atomic<float>* modRateParam      = nullptr;

    // Debug bypass switches
    std::atomic<float>* bypassEarlyParam      = nullptr;
    std::atomic<float>* bypassFDNParam        = nullptr;
    std::atomic<float>* bypassDVNParam        = nullptr;
    std::atomic<float>* bypassSaturationParam = nullptr;
    std::atomic<float>* bypassToneFilterParam = nullptr;
    std::atomic<float>* bypassAttenFilterParam = nullptr;
    std::atomic<float>* bypassModulationParam = nullptr;

    // DSP
    DSP::EarlyReflections earlyReflections[2];
    DSP::FDNReverb fdnReverb;
    DSP::DarkVelvetNoise dvnTail[2];
    DSP::OversamplingManager oversamplingManager;
    DSP::ReverbMixer reverbMixer;

    // Pre-delay
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelayLine[2];
    static constexpr int MAX_PRE_DELAY_SAMPLES = 4800;

    // Internal buffers
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> earlyBuffer;
    juce::AudioBuffer<float> fdnInputBuffer;
    juce::AudioBuffer<float> dvnBuffer;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int lastOversamplingFactor = -1;

    void updateParameters();
    void initializeOversampling (int factor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WetStringReverbProcessor)
};
