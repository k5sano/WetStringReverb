#pragma once

#include "DSP/DelayLine.h"
#include "DSP/FeedbackMatrix.h"
#include "DSP/AttenuationFilter.h"
#include "DSP/Saturation.h"
#include "DSP/SaturationToneFilter.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace DSP
{

class FDNReverb
{
public:
    static constexpr int NUM_CHANNELS = 8;

    static constexpr std::array<int, NUM_CHANNELS> BASE_DELAYS = {
        443, 557, 661, 769, 883, 1013, 1151, 1277
    };

    FDNReverb() = default;

    void prepare (double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;

        int maxDelay = static_cast<int> (1277 * 2.0 * (sampleRate / 44100.0)) + 64;
        for (int i = 0; i < NUM_CHANNELS; ++i)
            delayLines[i].prepare (maxDelay);

        for (auto& filter : attenuationFilters)
            filter.reset();

        for (auto& sat : saturators)
            sat.prepare (sampleRate);

        for (auto& tf : toneFilters)
            tf.prepare (sampleRate);

        lfoPhase = 0.0;
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

        // Delay lengths
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            float delayLength = static_cast<float> (BASE_DELAYS[i]) * roomSize
                              * static_cast<float> (sr / 44100.0);
            currentDelays[i] = delayLength;
            delayLines[i].setDelay (delayLength);
        }

        // Crossover: exponential mapping
        float crossoverHz = 20000.0f * std::pow (500.0f / 20000.0f,
                                                  hfDamping * 0.01f);

        // Frequency-dependent attenuation per delay line
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            float delaySec = currentDelays[i] / static_cast<float> (sr);
            float gLow  = std::pow (10.0f, -3.0f * delaySec
                                            / std::max (lowRT60,  0.05f));
            float gHigh = std::pow (10.0f, -3.0f * delaySec
                                            / std::max (highRT60, 0.05f));

            // Safety clamp: loop gain must never exceed 1
            gLow  = std::min (gLow,  0.9999f);
            gHigh = std::min (gHigh, 0.9999f);

            attenuationFilters[i].setCoefficients (gLow, gHigh, crossoverHz,
                                                    static_cast<float> (sr));
        }

        // Diffusion: controls feedback matrix vs identity
        // Applied with energy normalization to guarantee unitary operation.
        currentDiffusion = std::clamp (diffusion * 0.01f, 0.0f, 1.0f);

        // Saturation
        for (auto& sat : saturators)
            sat.setParameters (satAmount, satDrive, satType, satAsymmetry);

        // Tone filter
        for (auto& tf : toneFilters)
            tf.setTone (satTone);

        // Modulation
        currentModDepth = modDepth * 0.01f;
        currentModRate  = modRate;
        maxModSamples   = 4.0f;
    }

    void processSample (float inputL, float inputR,
                        float& outputL, float& outputR)
    {
        // Read from delay lines
        std::array<float, NUM_CHANNELS> delayOutputs;
        for (int i = 0; i < NUM_CHANNELS; ++i)
            delayOutputs[i] = delayLines[i].read();

        // Output tap (pre-feedback)
        outputL = 0.0f;
        outputR = 0.0f;
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            if (i % 2 == 0)
                outputL += delayOutputs[i];
            else
                outputR += delayOutputs[i];
        }
        float outputScale = 1.0f / static_cast<float> (NUM_CHANNELS / 2);
        outputL *= outputScale;
        outputR *= outputScale;

        // --- Feedback path ---

        // 1. Attenuation filter (frequency-dependent decay)
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

        // 2. Feedback matrix (unitary mixing)
        //    Full Hadamard when diffusion=1, identity when diffusion=0.
        //    Blended with energy normalization to preserve unitarity.
        std::array<float, NUM_CHANNELS> feedback;
        if (currentDiffusion < 0.001f)
        {
            // No mixing: parallel delays
            feedback = attenuated;
        }
        else if (currentDiffusion > 0.999f)
        {
            // Full Hadamard
            feedbackMatrix.process (attenuated, feedback);
        }
        else
        {
            // Blended: interpolate then normalise to preserve energy
            std::array<float, NUM_CHANNELS> fullMix;
            feedbackMatrix.process (attenuated, fullMix);

            // Compute input energy
            float energyIn = 0.0f;
            for (int i = 0; i < NUM_CHANNELS; ++i)
                energyIn += attenuated[i] * attenuated[i];

            // Blend
            for (int i = 0; i < NUM_CHANNELS; ++i)
                feedback[i] = (1.0f - currentDiffusion) * attenuated[i]
                             + currentDiffusion * fullMix[i];

            // Compute output energy and normalise
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

        // 3. Saturation (or bypass)
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

        // 4. Tone filter (or bypass)
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

        // 5. Modulation + delay line write
        float lfoInc = 2.0f * 3.14159265f * currentModRate
                     / static_cast<float> (sr);

        // Input gain: scale input to prevent overdriving the FDN.
        // 1/sqrt(N/2) for energy-matched injection into 8-channel FDN.
        constexpr float inputScale = 1.0f / 2.0f;

        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            if (!bypassModulation)
            {
                float phaseOffset = 2.0f * 3.14159265f * static_cast<float> (i)
                                  / static_cast<float> (NUM_CHANNELS);
                float mod = currentModDepth * maxModSamples
                          * std::sin (static_cast<float> (lfoPhase) + phaseOffset);
                delayLines[i].setDelay (currentDelays[i] + mod);
            }

            float inputSample = (i % 2 == 0) ? inputL : inputR;
            delayLines[i].write (inputSample * inputScale + processed[i]);
        }

        if (!bypassModulation)
        {
            lfoPhase += static_cast<double> (lfoInc);
            if (lfoPhase > 2.0 * 3.14159265358979323846)
                lfoPhase -= 2.0 * 3.14159265358979323846;
        }
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
        lfoPhase = 0.0;
    }

private:
    double sr = 44100.0;
    std::array<DelayLine, NUM_CHANNELS> delayLines;
    FeedbackMatrix feedbackMatrix;
    std::array<AttenuationFilter, NUM_CHANNELS> attenuationFilters;
    std::array<Saturation, NUM_CHANNELS> saturators;
    std::array<SaturationToneFilter, NUM_CHANNELS> toneFilters;

    std::array<float, NUM_CHANNELS> currentDelays {};
    float currentModDepth  = 0.0f;
    float currentModRate   = 0.5f;
    float maxModSamples    = 4.0f;
    float currentDiffusion = 0.8f;
    double lfoPhase = 0.0;

    bool bypassSaturation  = false;
    bool bypassToneFilter  = false;
    bool bypassAttenFilter = false;
    bool bypassModulation  = false;
};

}  // namespace DSP
