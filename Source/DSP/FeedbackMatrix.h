#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace DSP
{

/**
 * 8x8 Hadamard 行列（符号反転ランダム化付き）。
 *
 * H_1 = {1} から再帰展開:
 *   H_{2k} = [ H_k   H_k  ]
 *            [ H_k  -H_k  ]
 *
 * 1/sqrt(N) で正規化し、ユニタリ行列でエネルギー保存を保証。
 * Schlecht & Habets (2020) の散乱行列理論に基づく。
 */
class FeedbackMatrix
{
public:
    static constexpr int N = 8;

    FeedbackMatrix()
    {
        float h[N][N];
        h[0][0] = 1.0f;
        int size = 1;

        while (size < N)
        {
            for (int i = 0; i < size; ++i)
            {
                for (int j = 0; j < size; ++j)
                {
                    float val = h[i][j];
                    h[i][j + size]        =  val;
                    h[i + size][j]        =  val;
                    h[i + size][j + size] = -val;
                }
            }
            size *= 2;
        }

        float norm = 1.0f / std::sqrt(static_cast<float>(N));
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
                matrix[i][j] = h[i][j] * norm;

        uint32_t seed = 0x12345678u;
        for (int i = 0; i < N; ++i)
        {
            seed = seed * 1664525u + 1013904223u;
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
