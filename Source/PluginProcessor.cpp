#include "PluginProcessor.h"

WetStringReverbProcessor::WetStringReverbProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", Parameters::createParameterLayout())
{
    preDelayParam     = apvts.getRawParameterValue (Parameters::PRE_DELAY);
    decayParam        = apvts.getRawParameterValue (Parameters::DECAY);
    dampingParam      = apvts.getRawParameterValue (Parameters::DAMPING);
    bandwidthParam    = apvts.getRawParameterValue (Parameters::BANDWIDTH);
    sizeParam         = apvts.getRawParameterValue (Parameters::SIZE);
    mixParam          = apvts.getRawParameterValue (Parameters::MIX);
    inputDiff1Param   = apvts.getRawParameterValue (Parameters::INPUT_DIFF_1);
    inputDiff2Param   = apvts.getRawParameterValue (Parameters::INPUT_DIFF_2);
    decayDiff1Param   = apvts.getRawParameterValue (Parameters::DECAY_DIFF_1);
    decayDiff2Param   = apvts.getRawParameterValue (Parameters::DECAY_DIFF_2);
    modRateParam      = apvts.getRawParameterValue (Parameters::MOD_RATE);
    modDepthParam     = apvts.getRawParameterValue (Parameters::MOD_DEPTH);
    oversamplingParam = apvts.getRawParameterValue (Parameters::OVERSAMPLING);
}

void WetStringReverbProcessor::initOversampling (int factor)
{
    if (factor > 0)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            2, factor,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampler->initProcessing (static_cast<size_t> (currentBlockSize));
    }
    else
    {
        oversampler.reset();
    }

    double osRate = currentSampleRate * static_cast<double> (1 << factor);
    reverb.prepare (osRate, currentBlockSize * (1 << factor));

    float latency = (oversampler != nullptr) ? oversampler->getLatencyInSamples() : 0.0f;
    setLatencySamples (static_cast<int> (latency));

    currentOversamplingFactor = factor;
}

void WetStringReverbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    // Pre-delay
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1 };
    preDelayLine.reset();
    preDelayLine.prepare (spec);
    preDelayLine.setMaximumDelayInSamples (MAX_PRE_DELAY_SAMPLES);

    // Oversampling + reverb
    int osFactor = static_cast<int> (oversamplingParam->load());
    initOversampling (osFactor);
}

void WetStringReverbProcessor::releaseResources() {}

void WetStringReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    // Clear unused output channels
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // Mono → stereo
    if (numChannels == 1 && getTotalNumOutputChannels() >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    // Check oversampling change
    int osFactor = static_cast<int> (oversamplingParam->load());
    if (osFactor != currentOversamplingFactor)
    {
        suspendProcessing (true);
        initOversampling (osFactor);
        suspendProcessing (false);
    }

    // Read parameters
    float preDelayMs = preDelayParam->load();
    float decay      = decayParam->load();
    float damping    = dampingParam->load();
    float bandwidth  = bandwidthParam->load();
    float size       = sizeParam->load();
    float mix        = mixParam->load();
    float diff1      = inputDiff1Param->load();
    float modRate    = modRateParam->load();
    float modDepth   = modDepthParam->load();

    // Update reverb parameters
    reverb.setParameters (decay, damping, bandwidth, diff1, mix,
                          modRate, modDepth, size);

    // Pre-delay
    float preDelaySamples = preDelayMs * 0.001f * static_cast<float> (currentSampleRate);
    preDelayLine.setDelay (preDelaySamples);

    for (int ch = 0; ch < std::min (numChannels, 2); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            preDelayLine.pushSample (0, data[i]);
            data[i] = preDelayLine.popSample (0);
        }
    }

    // Oversampled reverb processing
    if (oversampler != nullptr)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto oversampledBlock = oversampler->processSamplesUp (block);

        int osNumSamples = static_cast<int> (oversampledBlock.getNumSamples());
        auto* osL = oversampledBlock.getChannelPointer (0);
        auto* osR = oversampledBlock.getChannelPointer (1);

        for (int i = 0; i < osNumSamples; ++i)
            reverb.processSample (osL[i], osR[i]);

        oversampler->processSamplesDown (block);
    }
    else
    {
        auto* dataL = buffer.getWritePointer (0);
        auto* dataR = numChannels >= 2 ? buffer.getWritePointer (1) : dataL;

        for (int i = 0; i < numSamples; ++i)
            reverb.processSample (dataL[i], dataR[i]);
    }
}

juce::AudioProcessorEditor* WetStringReverbProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void WetStringReverbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WetStringReverbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WetStringReverbProcessor();
}
