#pragma once

#include <cmath>
#include <algorithm>

namespace DSP
{

/**
 * 4 タイプの FDN 内サチュレーション。
 * FeedbackMatrix 直後、AttenuationFilter 直前に配置。
 */
class Saturation
{
public:
    enum class Type
    {
        Soft = 0,   // 3 次多項式ソフトクリップ
        Warm = 1,   // tanh(x) — デフォルト推奨
        Tape = 2,   // 正側 tanh / 負側 tanh(0.8x)*1.25
        Tube = 3    // 正負で異なる tanh ゲイン → 偶数次
    };

    Saturation() = default;

    void setParameters (float amountPercent, float driveDbs,
                        int typeIndex, float asymmetryPercent)
    {
        amount = amountPercent * 0.01f;
        driveLinear = std::pow (10.0f, driveDbs / 20.0f);
        type = static_cast<Type> (std::clamp (typeIndex, 0, 3));
        asymmetryOffset = asymmetryPercent * 0.002f;  // 0-100% → 0-0.2
    }

    void prepare (double sampleRate)
    {
        // DC ブロッカー (10Hz ハイパス)
        float fc = 10.0f / static_cast<float> (sampleRate);
        dcBlockCoeff = 1.0f - (2.0f * 3.14159265f * fc);
        dcBlockCoeff = std::max (0.9f, std::min (0.9999f, dcBlockCoeff));
        dcX1 = 0.0f;
        dcY1 = 0.0f;
    }

    float process (float input)
    {
        if (amount < 1.0e-6f)
            return input;  // 完全バイパス

        // Drive 適用
        float driven = input * driveLinear;

        // 非対称オフセット
        driven += asymmetryOffset;

        // 非線形関数適用
        float saturated = applyNonlinearity (driven);

        // DC ブロッカー（非対称オフセットが有効な場合のみ）
        float result;
        if (std::abs (asymmetryOffset) > 1.0e-6f)
        {
            float dcBlocked = saturated - dcX1 + dcBlockCoeff * dcY1;
            dcX1 = saturated;
            dcY1 = dcBlocked;
            result = dcBlocked;
        }
        else
        {
            result = saturated;
        }

        // Amount ブレンド
        return (1.0f - amount) * input + amount * result;
    }

    void reset()
    {
        // Pre-initialize DC blocker state so zero input produces zero output
        // even when asymmetry offset is non-zero
        float driven = asymmetryOffset;
        float saturated = applyNonlinearity (driven);
        dcX1 = saturated;
        dcY1 = 0.0f;
    }

private:
    float applyNonlinearity (float x) const
    {
        switch (type)
        {
            case Type::Soft:
            {
                // 3 次多項式ソフトクリップ: y = 1.5x - 0.5x^3 (|x| <= 1)
                float clamped = std::clamp (x, -1.0f, 1.0f);
                return 1.5f * clamped - 0.5f * clamped * clamped * clamped;
            }
            case Type::Warm:
            {
                return std::tanh (x);
            }
            case Type::Tape:
            {
                // 正側 tanh / 負側 tanh(0.8x)*1.25
                if (x >= 0.0f)
                    return std::tanh (x);
                else
                    return std::tanh (0.8f * x) * 1.25f;
            }
            case Type::Tube:
            {
                // 正側 tanh(1.2x) / 負側 tanh(0.8x)
                if (x >= 0.0f)
                    return std::tanh (1.2f * x);
                else
                    return std::tanh (0.8f * x);
            }
            default:
                return std::tanh (x);
        }
    }

    float amount = 0.0f;
    float driveLinear = 1.0f;
    Type type = Type::Warm;
    float asymmetryOffset = 0.0f;

    // DC ブロッカー
    float dcBlockCoeff = 0.995f;
    float dcX1 = 0.0f;
    float dcY1 = 0.0f;
};

}  // namespace DSP
