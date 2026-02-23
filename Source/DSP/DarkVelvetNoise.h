#pragma once

#include <vector>
#include <cmath>
#include <cstdint>

namespace DSP
{

/**
 * Dark Velvet Noise (DVN) テール処理（第 3 層）。
 * 非指数的減衰（ダブルスロープ）を実現。
 * 線形処理のためオーバーサンプリング不要。
 *
 * 参考: Fagerstrom et al., "Non-Exponential Reverb Modeling Using DVN", JAES 72(6) (2024)
 */
class DarkVelvetNoise
{
public:
    DarkVelvetNoise() = default;

    void prepare (double sampleRate, int maxBlockSize, uint32_t seed)
    {
        sr = sampleRate;
        generateDVNSequence (seed);

        // ランニングサムフィルタの状態
        runningSumStates.resize (numPulseWidths, 0.0f);
        inputRingBuffer.resize (static_cast<size_t> (maxBlockSize + dvnLength + 16), 0.0f);
        writePos = 0;
    }

    void setParameters (float decayShapePercent, float rt60Seconds)
    {
        decayShape = decayShapePercent * 0.01f;  // 0-1
        rt60 = rt60Seconds;
        updateEnvelopeCoefficients();
    }

    void process (const float* input, float* output, int numSamples, float gain)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            // 入力をリングバッファに書き込み
            inputRingBuffer[static_cast<size_t> (writePos)] = input[n];

            float sum = 0.0f;

            // DVN パルスによるスパース畳み込み
            for (const auto& pulse : dvnPulses)
            {
                int readIdx = writePos - pulse.position;
                if (readIdx < 0)
                    readIdx += static_cast<int> (inputRingBuffer.size());

                // パルス幅に応じたランニングサム（DVN のローパス特性）
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
        int width;      // パルス幅（1-8 サンプル）
        float envelope;
    };

    void generateDVNSequence (uint32_t seed)
    {
        // DVN パルス列を生成
        float density = 1800.0f;  // pulses/s
        int gridSize = std::max (1, static_cast<int> (sr / density));
        dvnLength = static_cast<int> (sr * 3.0);  // 最大 3 秒テール
        int numPulses = dvnLength / gridSize;

        dvnPulses.clear();
        dvnPulses.reserve (static_cast<size_t> (numPulses));

        uint32_t rng = seed;
        for (int m = 0; m < numPulses; ++m)
        {
            rng = rng * 1664525u + 1013904223u;
            int pos = m * gridSize + static_cast<int> (rng % static_cast<uint32_t> (gridSize));

            rng = rng * 1664525u + 1013904223u;
            float sign = (rng & 0x80000000u) ? -1.0f : 1.0f;

            // ランダムパルス幅（1-8）
            rng = rng * 1664525u + 1013904223u;
            int width = 1 + static_cast<int> (rng % 8u);

            if (pos < dvnLength)
                dvnPulses.push_back ({ pos, sign, width, 1.0f });
        }

        updateEnvelopeCoefficients();
    }

    void updateEnvelopeCoefficients()
    {
        if (dvnPulses.empty())
            return;

        float tau1 = rt60 / 6.9078f;      // 速い減衰時定数
        float tau2 = rt60 * 1.5f / 6.9078f; // 遅い減衰時定数（1.5 倍長い）

        for (auto& pulse : dvnPulses)
        {
            float t = static_cast<float> (pulse.position) / static_cast<float> (sr);
            // ダブルスロープ: (1 - shape) * exp(-t/tau1) + shape * exp(-t/tau2)
            pulse.envelope = (1.0f - decayShape) * std::exp (-t / (tau1 + 1.0e-6f))
                           + decayShape * std::exp (-t / (tau2 + 1.0e-6f));
        }
    }

    double sr = 44100.0;
    float decayShape = 0.4f;
    float rt60 = 1.8f;
    int dvnLength = 0;
    int numPulseWidths = 8;

    std::vector<DVNPulse> dvnPulses;
    std::vector<float> runningSumStates;
    std::vector<float> inputRingBuffer;
    int writePos = 0;
};

}  // namespace DSP
