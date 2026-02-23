#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <cstdint>

namespace DSP
{

/**
 * Optimized Velvet Noise (OVN) パルス列生成器。
 * スパース FIR フィルタとして初期反射を生成。
 *
 * 参考: Fagerström et al., "Velvet-Noise Feedback Delay Network", DAFx-20 (2020)
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
     * OVN パルス列を生成。
     * @param sampleRate サンプルレート
     * @param durationMs シーケンス長 (ms)
     * @param density パルス密度 (pulses/s)
     * @param seed 乱数シード（L/R で異なるシードを使用）
     */
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
            // グリッド内のランダム位置
            rng = rng * 1664525u + 1013904223u;
            int pos = m * gridSize + static_cast<int> (rng % static_cast<uint32_t> (gridSize));

            // ランダム符号
            rng = rng * 1664525u + 1013904223u;
            float sign = (rng & 0x80000000u) ? -1.0f : 1.0f;

            if (pos < totalSamples)
                pulses.push_back ({ pos, sign });
        }

        sequenceLength = totalSamples;

        // 指数減衰エンベロープ係数を事前計算
        decayRate = -3.0f * std::log (10.0f) / static_cast<float> (totalSamples);  // -60dB over duration
    }

    /** スパース FIR 畳み込み */
    void convolve (const float* input, float* output, int numSamples, float gain) const
    {
        // output をゼロクリア
        for (int i = 0; i < numSamples; ++i)
            output[i] = 0.0f;

        for (const auto& pulse : pulses)
        {
            float envelope = std::exp (decayRate * static_cast<float> (pulse.position));
            float coeff = pulse.sign * gain * envelope;

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
};

}  // namespace DSP
