#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace DSP
{

/**
 * Dry / Early Reflections / Late (FDN) / DVN Tail mixing.
 * v3: Called per-sample with smoothed parameters from the processor.
 *     Added denormal kill on output.
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

    void process (float dryL, float dryR,
                  float earlyL, float earlyR,
                  float lateL, float lateR,
                  float dvnL, float dvnR,
                  float& outL, float& outR) const
    {
        float wetL = earlyGain * earlyL + lateGain * (lateL + dvnL);
        float wetR = earlyGain * earlyR + lateGain * (lateR + dvnR);

        float mid  = (wetL + wetR) * 0.5f;
        float side = (wetL - wetR) * 0.5f;
        wetL = mid + side * stereoWidth;
        wetR = mid - side * stereoWidth;

        outL = softClip (dry * dryL + wet * wetL);
        outR = softClip (dry * dryR + wet * wetR);

        // Denormal kill
        outL = killDenormal (outL);
        outR = killDenormal (outR);
    }

private:
    static float softClip (float x)
    {
        if (x > 1.5f)  return 1.0f;
        if (x < -1.5f) return -1.0f;
        return x - (x * x * x) / 6.75f;
    }

    static float killDenormal (float x)
    {
        // Bit-level check: exponent field all zeros = denormal or zero
        union { float f; uint32_t i; } u;
        u.f = x;
        if ((u.i & 0x7F800000u) == 0)
            return 0.0f;
        return x;
    }

    float dry = 0.7f;
    float wet = 0.3f;
    float earlyGain = 0.707f;
    float lateGain = 0.5f;
    float stereoWidth = 0.7f;
};

}  // namespace DSP
