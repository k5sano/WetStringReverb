#pragma once

#include <cmath>
#include <algorithm>

namespace DSP
{

/**
 * Dry / Early Reflections / Late (FDN) / DVN Tail のミキシング。
 * ステレオ幅制御も担当。
 */
class ReverbMixer
{
public:
    ReverbMixer() = default;

    void setParameters (float dryWetPercent, float earlyLevelDb,
                        float lateLevelDb, float stereoWidthPercent)
    {
        wet = dryWetPercent * 0.01f;
        dry = 1.0f - wet;
        earlyGain = std::pow (10.0f, earlyLevelDb / 20.0f);
        lateGain = std::pow (10.0f, lateLevelDb / 20.0f);
        stereoWidth = stereoWidthPercent * 0.01f;
    }

    /**
     * 4 ソースをミキシングしてステレオ出力。
     */
    void process (float dryL, float dryR,
                  float earlyL, float earlyR,
                  float lateL, float lateR,
                  float dvnL, float dvnR,
                  float& outL, float& outR) const
    {
        // ウェット信号 = Early + Late + DVN
        float wetL = earlyGain * earlyL + lateGain * (lateL + dvnL);
        float wetR = earlyGain * earlyR + lateGain * (lateR + dvnR);

        // ステレオ幅制御（Mid/Side）
        float mid = (wetL + wetR) * 0.5f;
        float side = (wetL - wetR) * 0.5f;
        wetL = mid + side * stereoWidth;
        wetR = mid - side * stereoWidth;

        // Dry/Wet ミックス
        outL = dry * dryL + wet * wetL;
        outR = dry * dryR + wet * wetR;
    }

private:
    float dry = 0.7f;
    float wet = 0.3f;
    float earlyGain = 0.707f;
    float lateGain = 0.5f;
    float stereoWidth = 0.7f;
};

}  // namespace DSP
