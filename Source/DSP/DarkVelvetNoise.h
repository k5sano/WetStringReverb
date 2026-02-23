#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace DSP
{

/**
 * Dark Velvet Noise tail (Layer 3).
 *
 * Fixed: energy normalisation added so that the sparse FIR has
 * approximately unity RMS gain, matching Fagerström et al. (2020)
 * equation (15) normalisation requirement.
 */
class DarkVelvetNoise
{
public:
    DarkVelvetNoise() = default;

    void prepare (double sampleRate, int maxBlockSize, uint32_t seed)
    {
        sr = sampleRate;
        generateDVNSequence (seed);
        inputRingBuffer.resize (static_cast<size_t> (maxBlockSize + dvnLength + 16), 0.0f);
        writePos = 0;
    }

    void setParameters (float decayShapePercent, float rt60Seconds)
    {
        decayShape = decayShapePercent * 0.01f;
        rt60 = rt60Seconds;

        const double maxTailSec = std::min (3.0, std::max (0.1, static_cast<double> (rt60) * 2.0));
        dvnLength = static_cast<int> (sr * maxTailSec);

        updateEnvelopeCoefficients();
    }

    void process (const float* input, float* output, int numSamples, float gain)
    {
        const int ringSize = static_cast<int> (inputRingBuffer.size());

        for (int n = 0; n < numSamples; ++n)
        {
            int idx = (writePos + n) % ringSize;
            inputRingBuffer[static_cast<size_t> (idx)] = input[n];
        }

        std::fill (output, output + numSamples, 0.0f);

        for (const auto& pulse : dvnPulses)
        {
            // Apply pre-computed normalised coefficient
            const float coeff = pulse.sign * pulse.envelope * normGain;
            if (std::abs (coeff) < 1.0e-8f)
                continue;

            const int w = pulse.width;
            const float scaledCoeff = coeff / static_cast<float> (w);
            const int base = writePos - pulse.position;

            float windowSum = 0.0f;
            for (int j = 0; j < w; ++j)
            {
                int idx = ((base - j) % ringSize + ringSize) % ringSize;
                windowSum += inputRingBuffer[static_cast<size_t> (idx)];
            }

            output[0] += scaledCoeff * windowSum;

            for (int n = 1; n < numSamples; ++n)
            {
                int addIdx = ((base + n) % ringSize + ringSize) % ringSize;
                int remIdx = ((base + n - w) % ringSize + ringSize) % ringSize;
                windowSum += inputRingBuffer[static_cast<size_t> (addIdx)];
                windowSum -= inputRingBuffer[static_cast<size_t> (remIdx)];
                output[n] += scaledCoeff * windowSum;
            }
        }

        for (int n = 0; n < numSamples; ++n)
            output[n] *= gain;

        writePos = (writePos + numSamples) % ringSize;
    }

    void reset()
    {
        std::fill (inputRingBuffer.begin(), inputRingBuffer.end(), 0.0f);
        writePos = 0;
    }

private:
    struct DVNPulse
    {
        int position;
        float sign;
        int width;
        float envelope;
    };

    void generateDVNSequence (uint32_t seed)
    {
        const float density = 1800.0f;
        const int gridSize = std::max (1, static_cast<int> (sr / density));

        dvnLength = static_cast<int> (sr * 3.0);
        int numPulses = dvnLength / gridSize;
        numPulses = std::min (numPulses, 500);

        dvnPulses.clear();
        dvnPulses.reserve (static_cast<size_t> (numPulses));

        uint32_t rng = seed;
        for (int m = 0; m < numPulses; ++m)
        {
            rng = rng * 1664525u + 1013904223u;
            int pos = m * gridSize + static_cast<int> (rng % static_cast<uint32_t> (gridSize));

            rng = rng * 1664525u + 1013904223u;
            float sign = (rng & 0x80000000u) ? -1.0f : 1.0f;

            rng = rng * 1664525u + 1013904223u;
            int width = 1 + static_cast<int> (rng % 4u);

            if (pos < dvnLength)
                dvnPulses.push_back ({ pos, sign, width, 1.0f });
        }

        updateEnvelopeCoefficients();
    }

    void updateEnvelopeCoefficients()
    {
        if (dvnPulses.empty())
            return;

        float tau1 = rt60 / 6.9078f;
        float tau2 = rt60 * 1.5f / 6.9078f;

        // First pass: compute envelopes and accumulate energy
        float energySum = 0.0f;
        for (auto& pulse : dvnPulses)
        {
            if (pulse.position >= dvnLength)
            {
                pulse.envelope = 0.0f;
                continue;
            }

            float t = static_cast<float> (pulse.position) / static_cast<float> (sr);
            float env = (1.0f - decayShape) * std::exp (-t / (tau1 + 1.0e-6f))
                      + decayShape * std::exp (-t / (tau2 + 1.0e-6f));
            pulse.envelope = env;
            energySum += env * env;
        }

        // Normalise so the sparse filter has unity RMS gain
        normGain = (energySum > 1.0e-12f)
                 ? (1.0f / std::sqrt (energySum))
                 : 1.0f;
    }

    double sr = 44100.0;
    float decayShape = 0.4f;
    float rt60 = 1.8f;
    int dvnLength = 0;
    float normGain = 1.0f;

    std::vector<DVNPulse> dvnPulses;
    std::vector<float> inputRingBuffer;
    int writePos = 0;
};

}  // namespace DSP
