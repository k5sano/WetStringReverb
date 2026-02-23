#pragma once

#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace DSP
{

/**
 * FDN Core 専用のオーバーサンプリング管理。
 * juce::dsp::Oversampling を使用。
 */
class OversamplingManager
{
public:
    OversamplingManager() = default;

    /**
     * @param numChannels チャンネル数
     * @param factor 0=Off(1x), 1=2x, 2=4x
     */
    void prepare (int numChannels, int factor, double sampleRate, int maxBlockSize)
    {
        currentFactor = factor;
        channels = numChannels;

        if (factor > 0)
        {
            oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
                numChannels,
                factor,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                true  // maxQuality
            );
            oversampler->initProcessing (static_cast<size_t> (maxBlockSize));
        }
        else
        {
            oversampler.reset();
        }
    }

    juce::dsp::AudioBlock<float> processSamplesUp (juce::dsp::AudioBlock<float>& inputBlock)
    {
        if (oversampler != nullptr)
            return oversampler->processSamplesUp (inputBlock);
        return inputBlock;
    }

    void processSamplesDown (juce::dsp::AudioBlock<float>& outputBlock)
    {
        if (oversampler != nullptr)
            oversampler->processSamplesDown (outputBlock);
    }

    float getLatencyInSamples() const
    {
        if (oversampler != nullptr)
            return oversampler->getLatencyInSamples();
        return 0.0f;
    }

    double getOversampledRate (double baseRate) const
    {
        if (currentFactor == 0)
            return baseRate;
        return baseRate * static_cast<double> (1 << currentFactor);
    }

    int getFactor() const { return currentFactor; }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int currentFactor = 1;
    int channels = 2;
};

}  // namespace DSP
