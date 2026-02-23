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

/**
 * 8-channel Feedback Delay Network (Layer 2).
 * Runs at oversampled rate.
 *
 * Signal flow per sample (Jot 1992 / Schlecht & Habets 2020):
 *
 *   x_i(n) = b_i * input  (input injection, scaled by 1/sqrt(N/2))
 *
 *   s_i(n) = delayLine_i.read()                       [1. delay output]
 *   a_i(n) = attenuationFilter_i( s_i(n) )            [2. freq-dep decay]
 *   f(n)   = feedbackMatrix( a(n) )                    [3. unitary mixing]
 *   sat_i  = saturation_i( f_i(n) )                    [4. optional nonlinearity]
 *   t_i    = toneFilter_i( sat_i )                     [5. optional tone]
 *   delayLine_i.write( x_i(n) + t_i )                  [6. write back]
 *
 *   output = sum of c_i * a_i(n)                        [7. output tap, post-filter]
 *
 * References:
 *   Jot & Chaigne, "Digital delay networks for designing artificial reverberators",
 *       AES 90th Conv. (1991)
 *   Schlecht & Habets, "Scattering in FDNs", IEEE/ACM TASLP (2020)
 *   Dal Santo et al., "Optimizing Tiny Colorless FDNs", EURASIP JASM (2025)
 */
class FDNReverb
{
public:
    static constexpr int NUM_CHANNELS = 8;

    // Prime-based delays (@44.1 kHz). Mutually coprime for max mode density.
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

        // Crossover frequency (exponential mapping)
        // hfDamping 0%   -> 20 kHz (essentially no HF damping)
        // hfDamping 100% -> 500 Hz  (heavy HF damping)
        float crossoverHz = 20000.0f * std::pow (500.0f / 20000.0f,
                                                  hfDamping * 0.01f);

        // Per-channel attenuation filter (Jot design)
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            float delaySec = currentDelays[i] / static_cast<float> (sr);
            float gLow  = std::pow (10.0f, -3.0f * delaySec
                                            / std::max (lowRT60,  0.05f));
            float gHigh = std::pow (10.0f, -3.0f * delaySec
                                            / std::max (highRT60, 0.05f));

            // Clamp to prevent self-oscillation
            gLow  = std::min (gLow,  0.9999f);
            gHigh = std::min (gHigh, 0.9999f);

            attenuationFilters[i].setCoefficients (gLow, gHigh, crossoverHz,
                                                    static_cast<float> (sr));
        }

        // Diffusion: controls how much the feedback matrix mixes channels.
        // 0% = parallel comb filters (identity), 100% = full Householder.
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
        // --- 1. Read delay lines ---
        std::array<float, NUM_CHANNELS> delayOutputs;
        for (int i = 0; i < NUM_CHANNELS; ++i)
            delayOutputs[i] = delayLines[i].read();

        // --- 2. Attenuation filter (BEFORE matrix, per Jot) ---
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

        // --- 3. Output tap (post-attenuation, per Jot Fig.3.10) ---
        //    c_i = 1/sqrt(N/2) for L (even) and R (odd)
        constexpr float outputScale = 0.5f;  // 1/sqrt(4) = 0.5
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

        // --- 4. Feedback matrix (unitary mixing) ---
        std::array<float, NUM_CHANNELS> feedback;
        if (currentDiffusion < 0.001f)
        {
            // Identity: parallel comb filters
            feedback = attenuated;
        }
        else if (currentDiffusion > 0.999f)
        {
            // Full Householder
            feedbackMatrix.process (attenuated, feedback);
        }
        else
        {
            // Blended + energy-normalised to keep unitary
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

        // --- 5. Saturation (optional, inside feedback loop) ---
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

        // --- 6. Tone filter (optional) ---
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

        // --- 7. Modulation + write to delay lines ---
        float lfoInc = 2.0f * 3.14159265f * currentModRate
                     / static_cast<float> (sr);

        // Input gain: b_i = 1/sqrt(N/2)  (energy-matched injection)
        constexpr float inputScale = 0.5f;  // 1/sqrt(4) = 0.5

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
