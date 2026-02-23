#include "PluginProcessor.h"
#include "PluginEditor.h"

WetStringReverbProcessor::WetStringReverbProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", Parameters::createParameterLayout())
{
    // パラメータポインタの取得
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
}

void WetStringReverbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    // Pre-delay (each delay line is single-channel)
    juce::dsp::ProcessSpec spec { sampleRate,
                                   static_cast<juce::uint32> (samplesPerBlock),
                                   1 };
    for (auto& pd : preDelayLine)
    {
        pd.reset();
        pd.prepare (spec);
        pd.setMaximumDelayInSamples (static_cast<int> (sampleRate * 0.1) + 1);
    }

    // Early Reflections (L/R で異なるシード)
    earlyReflections[0].prepare (sampleRate, samplesPerBlock, 0xDEADBEEFu);
    earlyReflections[1].prepare (sampleRate, samplesPerBlock, 0xCAFEBABEu);

    // オーバーサンプリング + FDN
    int osFactor = static_cast<int> (oversamplingParam->load());
    initializeOversampling (osFactor);

    // DVN テール
    dvnTail[0].prepare (sampleRate, samplesPerBlock, 0xABCD1234u);
    dvnTail[1].prepare (sampleRate, samplesPerBlock, 0x5678EF01u);

    // 内部バッファの確保
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
    // リソース解放
}

void WetStringReverbProcessor::updateParameters()
{
    float roomSize    = roomSizeParam->load();
    float lowRT60     = lowRT60Param->load();
    float highRT60    = highRT60Param->load();
    float hfDamping   = hfDampingParam->load();
    float diffusion   = diffusionParam->load();
    float modDepth    = modDepthParam->load();
    float modRate     = modRateParam->load();
    float satAmount   = satAmountParam->load();
    float satDrive    = satDriveParam->load();
    int   satType     = static_cast<int> (satTypeParam->load());
    float satTone     = satToneParam->load();
    float satAsymmetry = satAsymmetryParam->load();

    fdnReverb.setParameters (roomSize, lowRT60, highRT60, hfDamping, diffusion,
                             modDepth, modRate,
                             satAmount, satDrive, satType, satTone, satAsymmetry);

    float decayShape  = decayShapeParam->load();
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

    // 未使用出力チャンネルのクリア
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    // モノ入力のステレオ化
    if (totalNumInputChannels == 1 && totalNumOutputChannels >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    // オーバーサンプリング倍率の変更チェック
    int osFactor = static_cast<int> (oversamplingParam->load());
    if (osFactor != lastOversamplingFactor)
    {
        suspendProcessing (true);
        initializeOversampling (osFactor);
        lastOversamplingFactor = osFactor;
        suspendProcessing (false);
    }

    // パラメータ更新
    updateParameters();

    // Dry 信号のコピー（事前確保バッファを使用）
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
    float earlyGain = std::pow (10.0f, earlyLevelParam->load() / 20.0f);
    for (int ch = 0; ch < 2 && ch < buffer.getNumChannels(); ++ch)
    {
        earlyReflections[ch].process (buffer.getReadPointer (ch),
                                       earlyBuffer.getWritePointer (ch),
                                       numSamples, earlyGain);
    }

    // FDN へのオーバーサンプリング処理
    for (int ch = 0; ch < 2 && ch < buffer.getNumChannels(); ++ch)
        fdnInputBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    juce::dsp::AudioBlock<float> fdnBlock (fdnInputBuffer);
    auto oversampledBlock = oversamplingManager.processSamplesUp (fdnBlock);

    // FDN 処理（オーバーサンプルレートで）
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

    // ダウンサンプル
    oversamplingManager.processSamplesDown (fdnBlock);

    // DVN テール
    float lateGain = std::pow (10.0f, lateLevelParam->load() / 20.0f);
    for (int ch = 0; ch < 2 && ch < buffer.getNumChannels(); ++ch)
    {
        dvnTail[ch].process (fdnInputBuffer.getReadPointer (ch),
                              dvnBuffer.getWritePointer (ch),
                              numSamples, lateGain);
    }

    // ミキシング
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() >= 2 ? buffer.getWritePointer (1) : outL;

    for (int i = 0; i < numSamples; ++i)
    {
        float mixL, mixR;
        reverbMixer.process (
            dryBuffer.getReadPointer (0)[i],
            dryBuffer.getNumChannels() >= 2 ? dryBuffer.getReadPointer (1)[i] : dryBuffer.getReadPointer (0)[i],
            earlyBuffer.getReadPointer (0)[i],
            earlyBuffer.getReadPointer (1)[i],
            fdnInputBuffer.getReadPointer (0)[i],
            fdnInputBuffer.getNumChannels() >= 2 ? fdnInputBuffer.getReadPointer (1)[i] : fdnInputBuffer.getReadPointer (0)[i],
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
