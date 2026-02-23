#pragma once

#include "DSP/VelvetNoise.h"

namespace DSP
{

/**
 * OVN early reflections (Layer 1).
 * Sparse FIR preserves string instrument transients.
 * Linear processing: no oversampling required.
 */
class EarlyReflections
{
public:
    EarlyReflections() = default;

    void prepare (double sampleRate, int maxBlockSize, uint32_t seed)
    {
        juce::ignoreUnused (maxBlockSize);
        sr = sampleRate;
        // Generate OVN pulse train (~30 ms, 2000 pulses/s)
        ovn.generate (sampleRate, 30.0f, 2000.0f, seed);
    }

    /**
     * Process early reflections.
     * @param input   input sample array
     * @param output  output sample array
     * @param numSamples  block size
     * @param gain    gain (linear, converted from dB externally)
     */
    void process (const float* input, float* output,
                  int numSamples, float gain)
    {
        ovn.convolve (input, output, numSamples, gain);
    }

    void reset()
    {
        // Ring buffer inside VelvetNoise will be cleared on next generate()
        // For a reset during playback, we would need VelvetNoise::reset()
        // but the current design regenerates on prepare().
    }

private:
    VelvetNoise ovn;
    double sr = 44100.0;
};

}  // namespace DSP
