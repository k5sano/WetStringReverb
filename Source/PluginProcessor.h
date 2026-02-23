#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "DSP/CloudsReverb.h"

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
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    // Parameter pointers
    std::atomic<float>* preDelayParam     = nullptr;
    std::atomic<float>* decayParam        = nullptr;
    std::atomic<float>* dampingParam      = nullptr;
    std::atomic<float>* bandwidthParam    = nullptr;
    std::atomic<float>* sizeParam         = nullptr;
    std::atomic<float>* mixParam          = nullptr;
    std::atomic<float>* inputDiff1Param   = nullptr;
    std::atomic<float>* inputDiff2Param   = nullptr;
    std::atomic<float>* decayDiff1Param   = nullptr;
    std::atomic<float>* decayDiff2Param   = nullptr;
    std::atomic<float>* modRateParam      = nullptr;
    std::atomic<float>* modDepthParam     = nullptr;
    std::atomic<float>* oversamplingParam = nullptr;

    // DSP
    DSP::CloudsReverb reverb;

    // Pre-delay
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelayLine;
    static constexpr int MAX_PRE_DELAY_SAMPLES = 9600;  // 200ms @ 48kHz

    // Oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int currentOversamplingFactor = -1;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    void initOversampling (int factor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WetStringReverbProcessor)
};
