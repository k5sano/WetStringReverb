#pragma once

#include <cmath>
#include <algorithm>

namespace DSP
{

/**
 * Post-saturation tone control.
 * Blend between lowpass (dark) and highpass emphasis (bright).
 *
 * FIXED: bright-side gain is clamped to prevent runaway inside
 * the FDN feedback loop.  The filter is guaranteed to have
 * |H(z)| <= 1 for all z on the unit circle.
 */
class SaturationToneFilter
{
public:
    SaturationToneFilter() = default;

    void prepare (double sampleRate)
    {
        sr = static_cast<float> (sampleRate);
        reset();
    }

    /**
     * @param tonePercent -100 to +100
     *   -100% = dark  (lowpass, fc sweeps down to 1 kHz)
     *      0% = flat   (bypass)
     *   +100% = bright (highpass emphasis, but gain-capped at unity)
     */
    void setTone (float tonePercent)
    {
        tone = tonePercent * 0.01f;  // -1 to +1

        if (std::abs (tone) < 0.01f)
        {
            isActive = false;
            return;
        }
        isActive = true;

        float freq;
        if (tone < 0.0f)
            freq = 1000.0f + (1.0f + tone) * 7000.0f;   // 1kHz-8kHz
        else
            freq = 8000.0f - tone * 4000.0f;              // 4kHz-8kHz

        freq = std::clamp (freq, 200.0f, sr * 0.49f);

        float w = 2.0f * 3.14159265f * freq / sr;
        lpCoeff = w / (1.0f + w);
    }

    float process (float input)
    {
        if (!isActive)
            return input;

        // First-order lowpass
        lpState += lpCoeff * (input - lpState);

        if (tone < 0.0f)
        {
            // Dark: crossfade toward lowpass output
            float blend = -tone;  // 0..1
            return (1.0f - blend) * input + blend * lpState;
            // |H| <= 1 always: LP has unity passband, crossfade is convex.
        }
        else
        {
            // Bright: subtract some low-frequency energy.
            // output = input - k * lp
            //
            // At DC  : |H| = 1 - k * 1 = 1 - k  (<= 1)
            // At Nyq : |H| = 1 - k * 0 = 1       (<= 1)
            //
            // This is always <= 1, so it cannot blow up in a feedback loop.
            float k = tone;  // 0..1
            return input - k * lpState;
        }
    }

    void reset()
    {
        lpState = 0.0f;
    }

private:
    float sr = 44100.0f;
    float tone = 0.0f;
    float lpCoeff = 0.1f;
    float lpState = 0.0f;
    bool isActive = false;
};

}  // namespace DSP
