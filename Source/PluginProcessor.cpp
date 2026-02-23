#include "PluginProcessor.h"
#include "PluginEditor.h"

WetStringReverbProcessor::WetStringReverbProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", Parameters::createParameterLayout())
{
    dryWetParam       = apvts.getRawParameterValue (Parameters::DRY_WET);
    preDelayParam     = apvts.getRawParameterValue (Parameters::PRE_DELAY_MS);
    earlyLevelParam   = apvts.getRawParameterValue (Parameters::EARLY_LEVEL_DB);
    lateLevelParam    = apvts.getRawParameterValue (Parameters::LATE_LEVEL_DB);
    roomSizeParam     = apvts.getRawParameterValue (Parameters::ROOM_SIZE);
    stereoWidthParam  = apvts.getRawParameterValue (Parameters::STEREO_WIDTH);
    oversamplingParam = apvts.getRawParameterValue (Parameters::OVERSAMPLING);

    lowRT60Param      = apvts.getRawParameterValue (Parameters::LOW_RT60_S);
    highRT60Param     = apvts.getRawParameterValue (Parameters::HIGH_RT60_S);
    hfDampingParam    = apvts.getRawParameterValue (Parameters::HF_DAMPING);
    diffusionParam    = apvts.getRawParameterValue (Parameters::DIFFUSION);
    decayShapeParam   = apvts.getRawParameterValue (Parameters::DECAY_SHAPE);

    satAmountParam    = apvts.getRawParameterValue (Parameters::SAT_AMOUNT);
    satDriveParam     = apvts.getRawParameterValue (Parameters::SAT_DRIVE_DB);
    satTypeParam      = apvts.getRawParameterValue (Parameters::SAT_TYPE);
    satToneParam      = apvts.getRawParameterValue (Parameters::SAT_TONE);
    satAsymmetryParam = apvts.getRawParameterValue (Parameters::SAT_ASYMMETRY);

    modDepthParam     = apvts.getRawParameterValue (Parameters::MOD_DEPTH);
    modRateParam      = apvts.getRawParameterValue (Parameters::MOD_RATE_HZ);

    // Debug bypass switches
    bypassEarlyParam      = apvts.getRawParameterValue (Parameters::BYPASS_EARLY);
    bypassFDNParam        = apvts.getRawParameterValue (Parameters::BYPASS_FDN);
    bypassDVNParam        = apvts.getRawParameterValue (Parameters::BYPASS_DVN);
    bypassSaturationParam = apvts.getRawParameterValue (Parameters::BYPASS_SATURATION);
    bypassToneFilterParam = apvts.getRawParameterValue (Parameters::BYPASS_TONE_FILTER);
    bypassAttenFilterParam = apvts.getRawParameterValue (Parameters::BYPASS_ATTEN_FILTER);
    bypassModulationParam = apvts.getRawParameterValue (Parameters::BYPASS_MODULATION);
}

void WetStringReverbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    juce::dsp::ProcessSpec spec { sampleRate,
                                   static_cast<juce::uint32> (samplesPerBlock),
                                   1 };
    for (auto& pd : preDelayLine)
    {
        pd.reset();
        pd.prepare (spec);
        pd.setMaximumDelayInSamples (static_cast<int> (sampleRate * 0.1) + 1);
    }

    earlyReflections[0].prepare (sampleRate, samplesPerBlock, 0xDEADBEEFu);
    earlyReflections[1].prepare (sampleRate, samplesPerBlock, 0xCAFEBABEu);

    int osFactor = static_cast<int> (oversamplingParam->load());
    initializeOversampling (osFactor);

    dvnTail[0].prepare (sampleRate, samplesPerBlock, 0xABCD1234u);
    dvnTail[1].prepare (sampleRate, samplesPerBlock, 0x5678EF01u);

    dryBuffer.setSize (2, samplesPerBlock);
    earlyBuffer.setSize (2, samplesPerBlock);
    fdnInputBuffer.setSize (2, samplesPerBlock);
    dvnBuffer.setSize (2, samplesPerBlock);

    lastOversamplingFactor = osFactor;
}

void WetStringReverbProcessor::initializeOversampling (int factor)
{
    oversamplingManager.prepare (2, factor, currentSampleRate, currentBlockSize);
    double osRate = oversamplingManager.getOversampledRate (currentSampleRate);
    int osBlockSize = currentBlockSize * (1 << factor);
    fdnReverb.prepare (osRate, osBlockSize);

    float totalLatency = oversamplingManager.getLatencyInSamples();
    setLatencySamples (static_cast<int> (totalLatency));
}

void WetStringReverbProcessor::releaseResources()
{
}

void WetStringReverbProcessor::updateParameters()
{
    float roomSize     = roomSizeParam->load();
    float lowRT60      = lowRT60Param->load();
    float highRT60     = highRT60Param->load();
    float hfDamping    = hfDampingParam->load();
    float diffusion    = diffusionParam->load();
    float modDepth     = modDepthParam->load();
    float modRate      = modRateParam->load();
    float satAmount    = satAmountParam->load();
    float satDrive     = satDriveParam->load();
    int   satType      = static_cast<int> (satTypeParam->load());
    float satTone      = satToneParam->load();
    float satAsymmetry = satAsymmetryParam->load();

    // Read bypass flags
    bool bSat   = bypassSaturationParam->load()  >= 0.5f;
    bool bTone  = bypassToneFilterParam->load()   >= 0.5f;
    bool bAtten = bypassAttenFilterParam->load()  >= 0.5f;
    bool bMod   = bypassModulationParam->load()   >= 0.5f;

    fdnReverb.setParameters (roomSize, lowRT60, highRT60, hfDamping, diffusion,
                             modDepth, modRate,
                             satAmount, satDrive, satType, satTone, satAsymmetry,
                             bSat, bTone, bAtten, bMod);

    float decayShape = decayShapeParam->load();
    dvnTail[0].setParameters (decayShape, lowRT60);
    dvnTail[1].setParameters (decayShape, lowRT60);

    float dryWet      = dryWetParam->load();
    float earlyLevel  = earlyLevelParam->load();
    float lateLevel   = lateLevelParam->load();
    float stereoWidth = stereoWidthParam->load();
    reverbMixer.setParameters (dryWet, earlyLevel, lateLevel, stereoWidth);
}

void WetStringReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (totalNumInputChannels == 1 && totalNumOutputChannels >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    int osFactor = static_cast<int> (oversamplingParam->load());
    if (osFactor != lastOversamplingFactor)
    {
        suspendProcessing (true);
        initializeOversampling (osFactor);
        lastOversamplingFactor = osFactor;
        suspendProcessing (false);
    }

    updateParameters();

    // Read layer bypass flags
    bool bypassEarly = bypassEarlyParam->load() >= 0.5f;
    bool bypassFDN   = bypassFDNParam->load()   >= 0.5f;
    bool bypassDVN   = bypassDVNParam->load()    >= 0.5f;

    // Dry copy
    for (int ch = 0; ch < 2 && ch < buffer.getNumChannels(); ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // Pre-Delay
    float preDelayMs = preDelayParam->load();
    float preDelaySamples = preDelayMs * 0.001f * static_cast<float> (currentSampleRate);
    for (int ch = 0; ch < 2 && ch < buffer.getNumChannels(); ++ch)
    {
        preDelayLine[ch].setDelay (preDelaySamples);
        auto* channelData = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float in = channelData[i];
            preDelayLine[ch].pushSample (0, in);
            channelData[i] = preDelayLine[ch].popSample (0);
        }
    }

    // Early Reflections
    if (bypassEarly)
    {
        earlyBuffer.clear (0, 0, numSamples);
        if (earlyBuffer.getNumChannels() >= 2)
            earlyBuffer.clear (1, 0, numSamples);
    }
    else
    {
        for (int ch = 0; ch < 2 && ch < buffer.getNumChannels(); ++ch)
        {
            earlyReflections[ch].process (buffer.getReadPointer (ch),
                                           earlyBuffer.getWritePointer (ch),
                                           numSamples, 1.0f);
        }
    }

    // FDN
    if (bypassFDN)
    {
        fdnInputBuffer.clear (0, 0, numSamples);
        if (fdnInputBuffer.getNumChannels() >= 2)
            fdnInputBuffer.clear (1, 0, numSamples);
    }
    else
    {
        for (int ch = 0; ch < 2 && ch < buffer.getNumChannels(); ++ch)
            fdnInputBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> fdnBlock (fdnInputBuffer);
        auto oversampledBlock = oversamplingManager.processSamplesUp (fdnBlock);

        int osNumSamples = static_cast<int> (oversampledBlock.getNumSamples());
        auto* osL = oversampledBlock.getChannelPointer (0);
        auto* osR = oversampledBlock.getChannelPointer (1);

        for (int i = 0; i < osNumSamples; ++i)
        {
            float outL, outR;
            fdnReverb.processSample (osL[i], osR[i], outL, outR);
            osL[i] = outL;
            osR[i] = outR;
        }

        oversamplingManager.processSamplesDown (fdnBlock);
    }

    // DVN Tail
    if (bypassDVN)
    {
        dvnBuffer.clear (0, 0, numSamples);
        if (dvnBuffer.getNumChannels() >= 2)
            dvnBuffer.clear (1, 0, numSamples);
    }
    else
    {
        for (int ch = 0; ch < 2 && ch < buffer.getNumChannels(); ++ch)
        {
            dvnTail[ch].process (fdnInputBuffer.getReadPointer (ch),
                                  dvnBuffer.getWritePointer (ch),
                                  numSamples, 1.0f);
        }
    }

    // Mixing
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() >= 2 ? buffer.getWritePointer (1) : outL;

    for (int i = 0; i < numSamples; ++i)
    {
        float mixL, mixR;
        reverbMixer.process (
            dryBuffer.getReadPointer (0)[i],
            dryBuffer.getNumChannels() >= 2 ? dryBuffer.getReadPointer (1)[i]
                                            : dryBuffer.getReadPointer (0)[i],
            earlyBuffer.getReadPointer (0)[i],
            earlyBuffer.getReadPointer (1)[i],
            fdnInputBuffer.getReadPointer (0)[i],
            fdnInputBuffer.getNumChannels() >= 2 ? fdnInputBuffer.getReadPointer (1)[i]
                                                 : fdnInputBuffer.getReadPointer (0)[i],
            dvnBuffer.getReadPointer (0)[i],
            dvnBuffer.getReadPointer (1)[i],
            mixL, mixR);
        outL[i] = mixL;
        if (outR != outL)
            outR[i] = mixR;
    }
}

juce::AudioProcessorEditor* WetStringReverbProcessor::createEditor()
{
    return new WetStringReverbEditor (*this);
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
