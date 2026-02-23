#pragma once

#include "DSP/DelayLine.h"
#include "DSP/FeedbackMatrix.h"
#include "DSP/AttenuationFilter.h"
#include "DSP/Saturation.h"
#include "DSP/SaturationToneFilter.h"
#include "DSP/Diffuser.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace DSP
{

/**
 * 8-channel Feedback Delay Network with input diffuser (Layer 2).
 * Runs at oversampled rate.
 *
 * v3 (2025): Internal one-pole smoothing on delay lengths and
 * attenuation coefficients to eliminate zipper noise when
 * automating Room Size or RT60.  Safety: hard energy ceiling
 * with per-channel output limiting.
 */
class FDNReverb
{
public:
    static constexpr int NUM_CHANNELS = 8;

    static constexpr std::array<int, NUM_CHANNELS> BASE_DELAYS = {
        887, 1151, 1559, 1907, 2467, 3109, 3907, 4787
    };

    FDNReverb() = default;

    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;

        // Smoothing coefficient: ~5ms time constant at oversampled rate
        smoothCoeff = 1.0f - std::exp (-1.0f / (static_cast<float> (sr) * 0.005f));

        int maxDelay = static_cast<int> (4787 * 2.0 * (sampleRate / 44100.0)) + 128;
        for (int i = 0; i < NUM_CHANNELS; ++i)
            delayLines[i].prepare (maxDelay);

        for (auto& filter : attenuationFilters)
            filter.reset();

        for (auto& sat : saturators)
            sat.prepare (sampleRate);

        for (auto& tf : toneFilters)
            tf.prepare (sampleRate);

        diffuser.prepare (sampleRate, maxBlockSize);

        lfoPhase = 0.0;
        energyAccum = 0.0f;
        energySampleCount = 0;
        reset();
    }

    void setParameters (float roomSize, float lowRT60, float highRT60,
                        float hfDamping, float diffusion,
                        float modDepth, float modRate,
                        float satAmount, float satDrive, int satType,
                        float satTone, float satAsymmetry,
                        bool bypSaturation  = false,
                        bool bypToneFilter  = false,
                        bool bypAttenFilter = false,
                        bool bypModulation  = false)
    {
        bypassSaturation  = bypSaturation;
        bypassToneFilter  = bypToneFilter;
        bypassAttenFilter = bypAttenFilter;
        bypassModulation  = bypModulation;

        // Target delay lengths (smoothing applied per-sample in processSample)
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            float delayLength = static_cast<float> (BASE_DELAYS[i]) * roomSize
                              * static_cast<float> (sr / 44100.0);
            targetDelays[i] = delayLength;
        }

        // Crossover frequency
        float crossoverHz = 20000.0f * std::pow (500.0f / 20000.0f,
                                                  hfDamping * 0.01f);

        // Compute target attenuation coefficients per channel
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            // Use target delay for coefficient calculation
            float delaySec = targetDelays[i] / static_cast<float> (sr);
            float gLow  = std::pow (10.0f, -3.0f * delaySec
                                            / std::max (lowRT60,  0.05f));
            float gHigh = std::pow (10.0f, -3.0f * delaySec
                                            / std::max (highRT60, 0.05f));

            // Hard ceiling: prevent any feedback gain >= 1
            gLow  = std::min (gLow,  0.9999f);
            gHigh = std::min (gHigh, 0.9999f);

            attenuationFilters[i].setCoefficients (gLow, gHigh, crossoverHz,
                                                    static_cast<float> (sr));
        }

        currentDiffusion = std::clamp (diffusion * 0.01f, 0.0f, 1.0f);

        for (auto& sat : saturators)
            sat.setParameters (satAmount, satDrive, satType, satAsymmetry);

        for (auto& tf : toneFilters)
            tf.setTone (satTone);

        currentModDepth = modDepth * 0.01f;
        currentModRate  = modRate;
        maxModSamples   = 16.0f;
    }

    void processSample (float inputL, float inputR,
                        float& outputL, float& outputR)
    {
        // --- 0. Input diffuser ---
        constexpr float inputScale = 0.5f;

        std::array<float, NUM_CHANNELS> diffuserInput;
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            float inputSample = (i % 2 == 0) ? inputL : inputR;
            diffuserInput[i] = inputSample * inputScale;
        }

        std::array<float, NUM_CHANNELS> diffused;
        diffuser.processSample (diffuserInput, diffused);

        // --- 1. Smooth delay lengths (one-pole) ---
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            currentDelays[i] += smoothCoeff * (targetDelays[i] - currentDelays[i]);
        }

        // --- 2. Read delay lines ---
        std::array<float, NUM_CHANNELS> delayOutputs;
        for (int i = 0; i < NUM_CHANNELS; ++i)
            delayOutputs[i] = delayLines[i].read();

        // --- 3. Attenuation filter ---
        std::array<float, NUM_CHANNELS> attenuated;
        if (bypassAttenFilter)
        {
            attenuated = delayOutputs;
        }
        else
        {
            for (int i = 0; i < NUM_CHANNELS; ++i)
                attenuated[i] = attenuationFilters[i].process (delayOutputs[i]);
        }

        // --- 4. Output tap ---
        constexpr float outputScale = 0.5f;
        outputL = 0.0f;
        outputR = 0.0f;
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            if (i % 2 == 0)
                outputL += attenuated[i];
            else
                outputR += attenuated[i];
        }
        outputL *= outputScale;
        outputR *= outputScale;

        // --- 5. Feedback matrix ---
        std::array<float, NUM_CHANNELS> feedback;
        if (currentDiffusion < 0.001f)
        {
            feedback = attenuated;
        }
        else if (currentDiffusion > 0.999f)
        {
            feedbackMatrix.process (attenuated, feedback);
        }
        else
        {
            std::array<float, NUM_CHANNELS> fullMix;
            feedbackMatrix.process (attenuated, fullMix);

            float energyIn = 0.0f;
            for (int i = 0; i < NUM_CHANNELS; ++i)
                energyIn += attenuated[i] * attenuated[i];

            for (int i = 0; i < NUM_CHANNELS; ++i)
                feedback[i] = (1.0f - currentDiffusion) * attenuated[i]
                             + currentDiffusion * fullMix[i];

            float energyOut = 0.0f;
            for (int i = 0; i < NUM_CHANNELS; ++i)
                energyOut += feedback[i] * feedback[i];

            if (energyOut > 1.0e-10f && energyIn > 1.0e-10f)
            {
                float norm = std::sqrt (energyIn / energyOut);
                for (int i = 0; i < NUM_CHANNELS; ++i)
                    feedback[i] *= norm;
            }
        }

        // --- 6. Saturation ---
        std::array<float, NUM_CHANNELS> afterSat;
        if (bypassSaturation)
        {
            afterSat = feedback;
        }
        else
        {
            for (int i = 0; i < NUM_CHANNELS; ++i)
                afterSat[i] = saturators[i].process (feedback[i]);
        }

        // --- 7. Tone filter ---
        std::array<float, NUM_CHANNELS> processed;
        if (bypassToneFilter)
        {
            processed = afterSat;
        }
        else
        {
            for (int i = 0; i < NUM_CHANNELS; ++i)
                processed[i] = toneFilters[i].process (afterSat[i]);
        }

        // --- 8. Safety limiter: per-channel soft clamp ---
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            float x = processed[i];
            if (x > 2.0f)       x = 2.0f * std::tanh (x * 0.5f);
            else if (x < -2.0f) x = 2.0f * std::tanh (x * 0.5f);
            processed[i] = x;
        }

        // --- 9. Modulation + write ---
        float lfoInc = 2.0f * 3.14159265f * currentModRate
                     / static_cast<float> (sr);

        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            float delayToSet = currentDelays[i];

            if (!bypassModulation)
            {
                float phaseOffset = 2.0f * 3.14159265f * static_cast<float> (i)
                                  / static_cast<float> (NUM_CHANNELS);
                float mod = currentModDepth * maxModSamples
                          * std::sin (static_cast<float> (lfoPhase) + phaseOffset);
                delayToSet += mod;
            }

            delayLines[i].setDelay (delayToSet);
            delayLines[i].write (diffused[i] + processed[i]);
        }

        if (!bypassModulation)
        {
            lfoPhase += static_cast<double> (lfoInc);
            if (lfoPhase > 2.0 * 3.14159265358979323846)
                lfoPhase -= 2.0 * 3.14159265358979323846;
        }

        // --- 10. Denormal kill on outputs ---
        outputL = killDenormal (outputL);
        outputR = killDenormal (outputR);
    }

    void reset()
    {
        for (auto& dl : delayLines)
            dl.clear();
        for (auto& f : attenuationFilters)
            f.reset();
        for (auto& s : saturators)
            s.reset();
        for (auto& tf : toneFilters)
            tf.reset();
        diffuser.reset();
        lfoPhase = 0.0;
        energyAccum = 0.0f;
        energySampleCount = 0;

        // Initialize smooth delays to target on reset
        for (int i = 0; i < NUM_CHANNELS; ++i)
            currentDelays[i] = targetDelays[i];
    }

private:
    static float killDenormal (float x)
    {
        static constexpr float antiDenormal = 1.0e-18f;
        x += antiDenormal;
        x -= antiDenormal;
        return x;
    }

    double sr = 44100.0;
    float smoothCoeff = 0.01f;

    std::array<DelayLine, NUM_CHANNELS> delayLines;
    FeedbackMatrix feedbackMatrix;
    std::array<AttenuationFilter, NUM_CHANNELS> attenuationFilters;
    std::array<Saturation, NUM_CHANNELS> saturators;
    std::array<SaturationToneFilter, NUM_CHANNELS> toneFilters;
    Diffuser diffuser;

    std::array<float, NUM_CHANNELS> targetDelays {};
    std::array<float, NUM_CHANNELS> currentDelays {};
    float currentModDepth  = 0.0f;
    float currentModRate   = 0.5f;
    float maxModSamples    = 16.0f;
    float currentDiffusion = 0.8f;
    double lfoPhase = 0.0;

    // Energy monitoring (for future metering / safety)
    float energyAccum = 0.0f;
    int energySampleCount = 0;

    bool bypassSaturation  = false;
    bool bypassToneFilter  = false;
    bool bypassAttenFilter = false;
    bool bypassModulation  = false;
};

}  // namespace DSP
