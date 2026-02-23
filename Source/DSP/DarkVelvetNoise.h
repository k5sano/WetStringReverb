#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace DSP
{

class DarkVelvetNoise
{
public:
    DarkVelvetNoise() = default;

    void prepare (double sampleRate, int maxBlockSize, uint32_t seed)
    {
        sr = sampleRate;
        generateDVNSequence (seed);
        runningSumStates.resize (numPulseWidths, 0.0f);
        inputRingBuffer.resize (static_cast<size_t> (maxBlockSize + dvnLength + 16), 0.0f);
        writePos = 0;
    }

    void setParameters (float decayShapePercent, float rt60Seconds)
    {
        decayShape = decayShapePercent * 0.01f;
        rt60 = rt60Seconds;

        // テール長を min(3秒, rt60*2) に制限
        const double maxTailSec = std::min (3.0, std::max (0.1, static_cast<double> (rt60) * 2.0));
        dvnLength = static_cast<int> (sr * maxTailSec);

        updateEnvelopeCoefficients();
    }

    void process (const float* input, float* output, int numSamples, float gain)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            inputRingBuffer[static_cast<size_t> (writePos)] = input[n];
            float sum = 0.0f;

            for (const auto& pulse : dvnPulses)
            {
                int readIdx = writePos - pulse.position;
                if (readIdx < 0)
                    readIdx += static_cast<int> (inputRingBuffer.size());

                float sample = 0.0f;
                for (int w = 0; w < pulse.width; ++w)
                {
                    int idx = readIdx - w;
                    if (idx < 0)
                        idx += static_cast<int> (inputRingBuffer.size());
                    sample += inputRingBuffer[static_cast<size_t> (idx)];
                }
                sample /= static_cast<float> (pulse.width);
                sum += pulse.sign * pulse.envelope * sample;
            }

            output[n] = sum * gain;
            writePos = (writePos + 1) % static_cast<int> (inputRingBuffer.size());
        }
    }

    void reset()
    {
        std::fill (inputRingBuffer.begin(), inputRingBuffer.end(), 0.0f);
        std::fill (runningSumStates.begin(), runningSumStates.end(), 0.0f);
        writePos = 0;
    }

private:
    struct DVNPulse
    {
        int position;
        float sign;
        int width; // 1..4
        float envelope;
    };

    void generateDVNSequence (uint32_t seed)
    {
        const float density = 1800.0f;
        const int gridSize = std::max (1, static_cast<int> (sr / density));

        // 初期長（setParametersで更新）
        dvnLength = static_cast<int> (sr * 3.0);
        int numPulses = dvnLength / gridSize;

        // パルス数上限 2000
        numPulses = std::min (numPulses, 2000);

        dvnPulses.clear();
        dvnPulses.reserve (static_cast<size_t> (numPulses));

        uint32_t rng = seed;
        for (int m = 0; m < numPulses; ++m)
        {
            rng = rng * 1664525u + 1013904223u;
            int pos = m * gridSize + static_cast<int> (rng % static_cast<uint32_t> (gridSize));

            rng = rng * 1664525u + 1013904223u;
            float sign = (rng & 0x80000000u) ? -1.0f : 1.0f;

            // パルス幅を 1..4 に制限
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

        for (auto& pulse : dvnPulses)
        {
            if (pulse.position >= dvnLength)
            {
                pulse.envelope = 0.0f;
                continue;
            }

            float t = static_cast<float> (pulse.position) / static_cast<float> (sr);
            pulse.envelope = (1.0f - decayShape) * std::exp (-t / (tau1 + 1.0e-6f))
                           + decayShape * std::exp (-t / (tau2 + 1.0e-6f));
        }
    }

    double sr = 44100.0;
    float decayShape = 0.4f;
    float rt60 = 1.8f;
    int dvnLength = 0;
    int numPulseWidths = 4;

    std::vector<DVNPulse> dvnPulses;
    std::vector<float> runningSumStates;
    std::vector<float> inputRingBuffer;
    int writePos = 0;
};

} // namespace DSP
