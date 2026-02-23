#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace DSP
{

/**
 * Optimised Velvet Noise (OVN) for early reflections.
 *
 * Uses a ring buffer so that pulses whose position exceeds the
 * current block size are still correctly applied across block
 * boundaries.  Energy is normalised so the sparse FIR has
 * approximately unity RMS gain.
 *
 * Reference:
 *   Valimaki et al., "Late Reverberation Synthesis Using
 *   Filtered Velvet Noise", JAES 60(3), 2012.
 */
class VelvetNoise
{
public:
    struct Pulse
    {
        int position;
        float sign;
    };

    VelvetNoise() = default;

    /**
     * Generate an OVN pulse sequence.
     * @param sampleRate   host sample rate (Hz)
     * @param durationMs   impulse response length (ms)
     * @param density      pulse density (pulses / second)
     * @param seed         LCG seed for reproducibility
     */
    void generate (double sampleRate, float durationMs,
                   float density, uint32_t seed)
    {
        sr = sampleRate;
        sequenceLength = static_cast<int> (sampleRate * durationMs * 0.001f);
        int gridSize = std::max (1, static_cast<int> (sampleRate / density));
        int numPulses = sequenceLength / gridSize;

        pulses.clear();
        pulses.reserve (static_cast<size_t> (numPulses));

        uint32_t rng = seed;
        for (int m = 0; m < numPulses; ++m)
        {
            rng = rng * 1664525u + 1013904223u;
            int pos = m * gridSize
                    + static_cast<int> (rng % static_cast<uint32_t> (gridSize));

            rng = rng * 1664525u + 1013904223u;
            float sign = (rng & 0x80000000u) ? -1.0f : 1.0f;

            if (pos < sequenceLength)
                pulses.push_back ({ pos, sign });
        }

        // -60 dB decay over the full duration
        decayRate = -3.0f * std::log (10.0f)
                  / std::max (1.0f, static_cast<float> (sequenceLength));

        // Pre-compute envelope-weighted coefficients and RMS normalisation
        float energySum = 0.0f;
        envelopes.resize (pulses.size());
        for (size_t k = 0; k < pulses.size(); ++k)
        {
            float env = std::exp (decayRate * static_cast<float> (pulses[k].position));
            envelopes[k] = env;
            energySum += env * env;
        }
        normGain = (energySum > 1.0e-6f) ? (1.0f / std::sqrt (energySum)) : 1.0f;

        // Allocate ring buffer large enough for the full sequence length
        ringSize = sequenceLength + 256;  // margin
        ringBuffer.assign (static_cast<size_t> (ringSize), 0.0f);
        ringWritePos = 0;
    }

    /**
     * Process one block of audio through the sparse FIR.
     * Uses a ring buffer so pulses at any position within the
     * sequence length can access past input samples.
     *
     * @param input    input sample array (numSamples)
     * @param output   output sample array (numSamples)
     * @param numSamples  block size
     * @param gain     overall gain multiplier
     */
    void convolve (const float* input, float* output,
                   int numSamples, float gain) const
    {
        // Write input into ring buffer (const_cast needed because the
        // ring buffer is logically mutable state, not output state)
        auto* ring = const_cast<float*> (ringBuffer.data());
        int wp = ringWritePos;

        for (int n = 0; n < numSamples; ++n)
        {
            ring[wp] = input[n];
            wp = (wp + 1) % ringSize;
        }

        // Clear output
        for (int n = 0; n < numSamples; ++n)
            output[n] = 0.0f;

        // Sparse FIR convolution via ring buffer
        for (size_t k = 0; k < pulses.size(); ++k)
        {
            float coeff = pulses[k].sign * gain * envelopes[k] * normGain;
            if (std::abs (coeff) < 1.0e-10f)
                continue;

            int pulsePos = pulses[k].position;

            for (int n = 0; n < numSamples; ++n)
            {
                // The sample we wrote at time (ringWritePos + n) needs
                // to be convolved with the pulse at offset pulsePos.
                // readIdx = (ringWritePos + n - pulsePos)
                int readIdx = (ringWritePos + n - pulsePos % ringSize + ringSize) % ringSize;
                output[n] += coeff * ring[readIdx];
            }
        }

        // Advance write position
        const_cast<int&> (ringWritePos) = wp;
    }

    int getSequenceLength() const { return sequenceLength; }
    const std::vector<Pulse>& getPulses() const { return pulses; }

private:
    double sr = 44100.0;
    std::vector<Pulse> pulses;
    std::vector<float> envelopes;
    int sequenceLength = 0;
    float decayRate = 0.0f;
    float normGain = 1.0f;

    // Ring buffer for cross-block convolution
    mutable std::vector<float> ringBuffer;
    mutable int ringWritePos = 0;
    int ringSize = 0;
};

}  // namespace DSP
