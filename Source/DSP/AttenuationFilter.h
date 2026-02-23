#pragma once

#include <cmath>
#include <algorithm>

namespace DSP
{

/**
 * First-order shelving filter for frequency-dependent attenuation.
 * Placed INSIDE the FDN feedback loop, BEFORE the feedback matrix
 * (per Jot 1992 / Schlecht 2018).
 *
 * Design ensures:
 *   H(z=1)  = gainLow   (DC gain)
 *   H(z=-1) = gainHigh  (Nyquist gain)
 *   |H(e^jw)| <= max(gainLow, gainHigh) for all w
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
            b0 = gainLow;
            b1 = 0.0f;
            a1Coeff = 0.0f;
            return;
        }

        float wc = 3.14159265f
                  * std::clamp (crossoverFreq, 20.0f, sampleRate * 0.49f)
                  / sampleRate;
        float t  = std::tan (wc);
        float ap = (t - 1.0f) / (t + 1.0f);

        // H(1)  = (b0+b1)/(1+a1) = gainLow
        // H(-1) = (b0-b1)/(1-a1) = gainHigh
        b0 = 0.5f * (gainLow * (1.0f + ap) + gainHigh * (1.0f - ap));
        b1 = 0.5f * (gainLow * (1.0f + ap) - gainHigh * (1.0f - ap));
        a1Coeff = ap;
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
    float z1 = 0.0f;
    float zOut1 = 0.0f;
};

}  // namespace DSP
