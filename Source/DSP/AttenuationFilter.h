#pragma once

#include <cmath>
#include <algorithm>

namespace DSP
{

/**
 * First-order shelving filter for frequency-dependent attenuation.
 * v3: Additional safety for extended RT60 ranges (up to 12s).
 *     Coefficient smoothing via one-pole to avoid clicks.
 */
class AttenuationFilter
{
public:
    AttenuationFilter() = default;

    void setCoefficients (float gainLow, float gainHigh,
                          float crossoverFreq, float sampleRate)
    {
        // Hard clamp: loop gain must never exceed 1
        gainLow  = std::clamp (gainLow,  0.0f, 0.9999f);
        gainHigh = std::clamp (gainHigh, 0.0f, 0.9999f);

        if (std::abs (gainLow - gainHigh) < 1.0e-6f)
        {
            targetB0 = gainLow;
            targetB1 = 0.0f;
            targetA1 = 0.0f;
            return;
        }

        float wc = 3.14159265f
                  * std::clamp (crossoverFreq, 20.0f, sampleRate * 0.49f)
                  / sampleRate;
        float t  = std::tan (wc);
        float ap = (t - 1.0f) / (t + 1.0f);

        targetB0 = 0.5f * (gainLow * (1.0f + ap) + gainHigh * (1.0f - ap));
        targetB1 = 0.5f * (gainLow * (1.0f + ap) - gainHigh * (1.0f - ap));
        targetA1 = ap;
    }

    float process (float input)
    {
        // One-pole smoothing on coefficients (~2ms at 44.1kHz)
        constexpr float smooth = 0.005f;
        b0 += smooth * (targetB0 - b0);
        b1 += smooth * (targetB1 - b1);
        a1Coeff += smooth * (targetA1 - a1Coeff);

        float output = b0 * input + b1 * z1 - a1Coeff * zOut1;
        z1 = input;
        zOut1 = output;

        // Denormal protection on filter state
        if (std::abs (zOut1) < 1.0e-18f) zOut1 = 0.0f;

        return output;
    }

    void reset()
    {
        z1 = 0.0f;
        zOut1 = 0.0f;
        // Snap coefficients to target immediately
        b0 = targetB0;
        b1 = targetB1;
        a1Coeff = targetA1;
    }

private:
    // Current (smoothed) coefficients
    float b0 = 1.0f;
    float b1 = 0.0f;
    float a1Coeff = 0.0f;

    // Target coefficients
    float targetB0 = 1.0f;
    float targetB1 = 0.0f;
    float targetA1 = 0.0f;

    // Filter state
    float z1 = 0.0f;
    float zOut1 = 0.0f;
};

}  // namespace DSP
