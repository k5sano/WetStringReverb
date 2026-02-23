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
        const int ringSize = static_cast<int> (inputRingBuffer.size());

        // ブロック分の入力をリングバッファに先行コピー
        for (int n = 0; n < numSamples; ++n)
        {
            int idx = (writePos + n) % ringSize;
            inputRingBuffer[static_cast<size_t> (idx)] = input[n];
        }

        // 出力バッファをゼロクリア
        std::fill (output, output + numSamples, 0.0f);

        // pulse-outer → sample-inner（キャッシュ効率改善）
        // width=1 固定のため直接1サンプル参照
        for (const auto& pulse : dvnPulses)
        {
            const float coeff = pulse.sign * pulse.envelope;
            if (std::abs (coeff) < 1.0e-8f)
                continue;

            const int base = writePos - pulse.position;

            for (int n = 0; n < numSamples; ++n)
            {
                int idx = ((base + n) % ringSize + ringSize) % ringSize;
                output[n] += coeff * inputRingBuffer[static_cast<size_t> (idx)];
            }
        }

        // ゲイン適用
        for (int n = 0; n < numSamples; ++n)
            output[n] *= gain;

        // ライトポジションをブロック分進める
        writePos = (writePos + numSamples) % ringSize;
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

        // パルス数上限 500（知覚的に十分な密度 ~167パルス/秒）
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

            // パルス幅を 1 に固定（width ループ排除）
            int width = 1;

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
