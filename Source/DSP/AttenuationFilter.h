#pragma once

#include <cmath>

namespace DSP
{

/**
 * 1 次シェルビングフィルタ（周波数依存減衰）。
 * 各 FDN ディレイライン後段に配置し、
 * 低域と高域で異なる RT60 を実現する。
 *
 * H(z) = (b0 + b1 * z^-1) / (1 + a1 * z^-1)
 */
class AttenuationFilter
{
public:
    AttenuationFilter() = default;

    /**
     * フィルタ係数を設定。
     * @param gainLow  低域ループゲイン（0–1）
     * @param gainHigh 高域ループゲイン（0–1）
     * @param crossoverFreq クロスオーバー周波数 (Hz)
     * @param sampleRate サンプルレート (Hz)
     */
    void setCoefficients (float gainLow, float gainHigh,
                          float crossoverFreq, float sampleRate)
    {
        // 1 次シェルビングフィルタ設計
        // Moorer/Jot 方式の簡略化版
        float wc = 2.0f * 3.14159265f * crossoverFreq / sampleRate;
        float t = std::tan (wc * 0.5f);

        // 低域ゲインと高域ゲインの比
        float gRatio = gainHigh / (gainLow + 1.0e-12f);

        // 係数計算
        float k = (1.0f - gRatio * t) / (1.0f + gRatio * t);

        b0 = gainLow * (1.0f + k) * 0.5f + gainLow * (1.0f - k) * 0.5f * gRatio;
        b1 = gainLow * (1.0f - k) * 0.5f + gainLow * (1.0f + k) * 0.5f * gRatio;

        // より安定な近似: Jot の 1 次減衰フィルタ
        // g_dc = gainLow, g_pi = gainHigh
        float p = (gainLow + gainHigh) * 0.5f;
        float q = (gainLow - gainHigh) * 0.5f;

        // 1 次 IIR: H(z) = p + q * (1 + a1*z^-1) / ...
        // 簡略版: 直接 1 次ローパスブレンド
        float alpha = std::max (0.0f, std::min (1.0f,
            crossoverFreq / (sampleRate * 0.5f)));

        b0 = gainLow * (1.0f - alpha) + gainHigh * alpha;
        b1 = gainLow * alpha + gainHigh * (1.0f - alpha);
        a1Coeff = -(2.0f * alpha - 1.0f);

        // Jot 方式の正確な実装
        // 参考: Schlecht (2018) PhD thesis Eq. 3.24
        float cos_wc = std::cos (wc);
        float gL = gainLow;
        float gH = gainHigh;

        // a1 を決定（クロスオーバーでの応答条件から）
        if (std::abs (gL - gH) < 1.0e-6f)
        {
            // 低域と高域のゲインが同じ場合、単純なスカラー乗算
            b0 = gL;
            b1 = 0.0f;
            a1Coeff = 0.0f;
        }
        else
        {
            float c = (gH * gH - gL * gL * t * t) / (gH * gH + gL * gL * t * t + 1.0e-12f);
            a1Coeff = -c;
            b0 = 0.5f * ((gL + gH) + (gL - gH) * c);
            b1 = 0.5f * ((gL + gH) - (gL - gH) * c);
        }
    }

    float process (float input)
    {
        float output = b0 * input + b1 * z1 - a1Coeff * zOut1;
        z1 = input;
        zOut1 = output;
        return output;
    }

    void reset()
    {
        z1 = 0.0f;
        zOut1 = 0.0f;
    }

private:
    float b0 = 1.0f;
    float b1 = 0.0f;
    float a1Coeff = 0.0f;
    float z1 = 0.0f;      // 入力遅延レジスタ
    float zOut1 = 0.0f;    // 出力遅延レジスタ
};

}  // namespace DSP
