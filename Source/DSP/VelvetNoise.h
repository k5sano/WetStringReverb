#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace DSP
{

class VelvetNoise
{
public:
    struct Pulse
    {
        int position;
        float sign;
    };

    VelvetNoise() = default;

    void generate (double sampleRate, float durationMs, float density, uint32_t seed)
    {
        int totalSamples = static_cast<int> (sampleRate * durationMs * 0.001f);
        int gridSize = std::max (1, static_cast<int> (sampleRate / density));
        int numPulses = totalSamples / gridSize;

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

            if (pos < totalSamples)
                pulses.push_back ({ pos, sign });
        }

        sequenceLength = totalSamples;

        // -60dB decay over the full duration
        decayRate = -3.0f * std::log (10.0f)
                  / std::max (1.0f, static_cast<float> (totalSamples));

        // Energy normalisation: ensure the sparse FIR has unity peak gain.
        // Sum of absolute envelope-weighted coefficients:
        float absSum = 0.0f;
        for (const auto& pulse : pulses)
        {
            float env = std::exp (decayRate * static_cast<float> (pulse.position));
            absSum += std::abs (env);
        }
        // Normalise so that worst-case (all pulses in phase) sums to 1.0
        normGain = (absSum > 1.0e-6f) ? (1.0f / absSum) : 1.0f;
    }

    void convolve (const float* input, float* output,
                   int numSamples, float gain) const
    {
        for (int i = 0; i < numSamples; ++i)
            output[i] = 0.0f;

        for (const auto& pulse : pulses)
        {
            float envelope = std::exp (decayRate
                           * static_cast<float> (pulse.position));
            float coeff = pulse.sign * gain * envelope * normGain;

            for (int n = 0; n < numSamples; ++n)
            {
                int readPos = n - pulse.position;
                if (readPos >= 0 && readPos < numSamples)
                    output[n] += coeff * input[readPos];
            }
        }
    }

    int getSequenceLength() const { return sequenceLength; }
    const std::vector<Pulse>& getPulses() const { return pulses; }

private:
    std::vector<Pulse> pulses;
    int sequenceLength = 0;
    float decayRate = 0.0f;
    float normGain = 1.0f;
};

}  // namespace DSP
