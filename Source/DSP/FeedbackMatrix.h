#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace DSP
{

/**
 * 8x8 Householder 反射行列。
 *
 * A = I - (2/N) * ones(N,N)
 *
 * N=8 の場合:
 *   対角要素: 1 - 2/8 = 0.75
 *   非対角要素: -2/8 = -0.25
 *
 * ユニタリ行列でエネルギー保存を保証。
 * Schlecht & Habets (2020) の散乱行列理論に基づく。
 */
class FeedbackMatrix
{
public:
    static constexpr int N = 8;

    FeedbackMatrix()
    {
        // Householder 行列の初期化
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
                matrix[i][j] = (i == j) ? (1.0f - 2.0f / static_cast<float> (N))
                                        : (-2.0f / static_cast<float> (N));

        // 入出力ゲインにランダム符号を付与（位相ランダム化）
        // 固定シードで再現性を確保
        uint32_t seed = 0x12345678u;
        for (int i = 0; i < N; ++i)
        {
            seed = seed * 1664525u + 1013904223u;  // LCG
            inputSigns[i] = (seed & 0x80000000u) ? -1.0f : 1.0f;
            seed = seed * 1664525u + 1013904223u;
            outputSigns[i] = (seed & 0x80000000u) ? -1.0f : 1.0f;
        }
    }

    /**
     * 8 チャンネルの入力を行列乗算で処理。
     * output[i] = outputSigns[i] * Σ_j (matrix[i][j] * inputSigns[j] * input[j])
     */
    void process (const std::array<float, N>& input,
                  std::array<float, N>& output) const
    {
        for (int i = 0; i < N; ++i)
        {
            float sum = 0.0f;
            for (int j = 0; j < N; ++j)
                sum += matrix[i][j] * inputSigns[j] * input[j];
            output[i] = outputSigns[i] * sum;
        }
    }

private:
    float matrix[N][N] {};
    std::array<float, N> inputSigns {};
    std::array<float, N> outputSigns {};
};

}  // namespace DSP
