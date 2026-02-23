#pragma once
// ============================================================================
// Clouds Plate Reverb — faithful port from Mutable Instruments Clouds
// Original: Copyright 2014 Emilie Gillet (MIT License)
//
// Griesinger / Dattorro topology:
//   4 input allpass diffusers → figure-8 tank (2 × [2 AP + 1 delay])
//   2 LFOs for modulation (smear + tank chorus)
//
// Ported to plain C++ / JUCE with no stmlib dependency.
// Uses 32-bit float (no 12-bit compression).
// ============================================================================

#include <vector>
#include <cmath>
#include <algorithm>

namespace DSP
{

class CloudsReverb
{
public:
    CloudsReverb() = default;

    void prepare (double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;
        const double ratio = sampleRate / 32000.0;

        // Allocate one big flat buffer (like Clouds' 16384-sample arena)
        // Total delay memory needed (at 32 kHz reference):
        //   113+162+241+399+1653+2038+3411+1913+1663+4782 = 16375
        // Scale by ratio and add margin.
        int totalSamples = static_cast<int> (16384.0 * ratio) + 64;
        buffer.assign (static_cast<size_t> (totalSamples), 0.0f);
        bufferSize = totalSamples;

        // Compute delay line bases and lengths (scaled)
        static constexpr int baseLengths[NUM_DELAYS] = {
            113, 162, 241, 399, 1653, 2038, 3411, 1913, 1663, 4782
        };
        int offset = 0;
        for (int i = 0; i < NUM_DELAYS; ++i)
        {
            dl[i].length = std::max (1, static_cast<int> (baseLengths[i] * ratio));
            dl[i].base   = offset;
            offset += dl[i].length + 1;  // +1 for interpolation margin
        }
        // Make sure buffer is large enough
        if (offset > bufferSize)
        {
            bufferSize = offset + 64;
            buffer.assign (static_cast<size_t> (bufferSize), 0.0f);
        }

        writePtr = 0;

        // LFO frequencies (Clouds: 0.5/32000 and 0.3/32000 cycles/sample)
        lfo1Inc = 0.5 / sampleRate;
        lfo2Inc = 0.3 / sampleRate;
        lfo1Phase = 0.0;
        lfo2Phase = 0.0;
        lfoCounter = 0;
        lfo1Val = 0.0f;
        lfo2Val = 0.0f;

        lpDecay1 = 0.0f;
        lpDecay2 = 0.0f;
    }

    void setParameters (float decay, float damping, float inputBW,
                        float diffusion, float amount, float modRate,
                        float modDepth, float size)
    {
        // Map parameters to Clouds internal variables
        reverbTime  = decay;                          // 0..0.9999
        diffusion_  = diffusion;                      // kap: 0..1
        lp_         = 1.0f - damping;                 // klp: 0 = full damp, 1 = none
        amount_     = amount;                         // wet mix: 0..1
        inputGain_  = 0.2f;                           // fixed, like Clouds
        bandwidth_  = inputBW;                        // input lowpass

        // Modulation: scale LFO frequencies by modRate, amplitude by modDepth
        modRateScale  = std::max (0.01f, modRate);
        modDepthScale = modDepth;

        sizeScale     = std::max (0.5f, std::min (2.0f, size));
    }

    void processSample (float& left, float& right)
    {
        // --- Advance write pointer (Clouds decrements) ---
        writePtr--;
        if (writePtr < 0) writePtr += bufferSize;

        // --- LFO (Clouds updates every 32 samples; we do every sample with correct freq) ---
        lfoCounter++;
        if ((lfoCounter & 31) == 0)
        {
            lfo1Val = std::cos (static_cast<float> (lfo1Phase * 2.0 * PI));
            lfo2Val = std::cos (static_cast<float> (lfo2Phase * 2.0 * PI));
        }
        lfo1Phase += lfo1Inc * static_cast<double> (modRateScale);
        if (lfo1Phase >= 1.0) lfo1Phase -= 1.0;
        lfo2Phase += lfo2Inc * static_cast<double> (modRateScale);
        if (lfo2Phase >= 1.0) lfo2Phase -= 1.0;

        // Shorthand
        const float kap = diffusion_;
        const float klp = lp_;
        const float krt = reverbTime;

        float acc = 0.0f;
        float previousRead = 0.0f;
        float wet = 0.0f;
        float apout = 0.0f;

        // ---- Input bandwidth filter ----
        float inputMono = (left + right);
        bwState += bandwidth_ * (inputMono - bwState);
        inputMono = bwState;

        // ==== EXACT CLOUDS SIGNAL FLOW (line by line) ====

        // c.Interpolate(ap1, 10.0f, LFO_1, 60.0f, 1.0f);
        acc = 0.0f;
        {
            float offset = 10.0f * sizeF() + modDepthScale * 60.0f * sizeF() * lfo1Val;
            previousRead = interpolateDelay (AP1, offset);
            acc += previousRead * 1.0f;
        }

        // c.Write(ap1, 100, 0.0f);
        writeDelay (AP1, static_cast<int> (100.0f * sizeF()), acc);
        acc *= 0.0f;  // scale = 0.0

        // c.Read(in_out->l + in_out->r, gain);
        acc += inputMono * inputGain_;

        // c.Read(ap1 TAIL, kap);
        {
            float r = readDelayTail (AP1);
            previousRead = r;
            acc += r * kap;
        }
        // c.WriteAllPass(ap1, -kap);
        writeDelay (AP1, 0, acc);
        acc = acc * (-kap) + previousRead;

        // c.Read(ap2 TAIL, kap);
        {
            float r = readDelayTail (AP2);
            previousRead = r;
            acc += r * kap;
        }
        // c.WriteAllPass(ap2, -kap);
        writeDelay (AP2, 0, acc);
        acc = acc * (-kap) + previousRead;

        // c.Read(ap3 TAIL, kap);
        {
            float r = readDelayTail (AP3);
            previousRead = r;
            acc += r * kap;
        }
        // c.WriteAllPass(ap3, -kap);
        writeDelay (AP3, 0, acc);
        acc = acc * (-kap) + previousRead;

        // c.Read(ap4 TAIL, kap);
        {
            float r = readDelayTail (AP4);
            previousRead = r;
            acc += r * kap;
        }
        // c.WriteAllPass(ap4, -kap);
        writeDelay (AP4, 0, acc);
        acc = acc * (-kap) + previousRead;

        // c.Write(apout);
        apout = acc;

        // ==== Tank half-loop 1 ====

        // c.Load(apout);
        acc = apout;

        // c.Interpolate(del2, 4680.0f, LFO_2, 100.0f, krt);
        {
            float offset = 4680.0f * sizeF() + modDepthScale * 100.0f * sizeF() * lfo2Val;
            previousRead = interpolateDelay (DEL2, offset);
            acc += previousRead * krt;
        }

        // c.Lp(lp_1, klp);
        lpDecay1 += klp * (acc - lpDecay1);
        acc = lpDecay1;

        // c.Read(dap1a TAIL, -kap);
        {
            float r = readDelayTail (DAP1A);
            previousRead = r;
            acc += r * (-kap);
        }
        // c.WriteAllPass(dap1a, kap);
        writeDelay (DAP1A, 0, acc);
        acc = acc * kap + previousRead;

        // c.Read(dap1b TAIL, kap);
        {
            float r = readDelayTail (DAP1B);
            previousRead = r;
            acc += r * kap;
        }
        // c.WriteAllPass(dap1b, -kap);
        writeDelay (DAP1B, 0, acc);
        acc = acc * (-kap) + previousRead;

        // c.Write(del1, 2.0f);
        writeDelay (DEL1, 0, acc);
        acc *= 2.0f;

        // c.Write(wet, 0.0f);
        wet = acc;
        acc *= 0.0f;

        left += (wet - left) * amount_;

        // ==== Tank half-loop 2 ====

        // c.Load(apout);
        acc = apout;

        // c.Read(del1 TAIL, krt);
        {
            float r = readDelayTail (DEL1);
            previousRead = r;
            acc += r * krt;
        }

        // c.Lp(lp_2, klp);
        lpDecay2 += klp * (acc - lpDecay2);
        acc = lpDecay2;

        // c.Read(dap2a TAIL, kap);
        {
            float r = readDelayTail (DAP2A);
            previousRead = r;
            acc += r * kap;
        }
        // c.WriteAllPass(dap2a, -kap);
        writeDelay (DAP2A, 0, acc);
        acc = acc * (-kap) + previousRead;

        // c.Read(dap2b TAIL, -kap);
        {
            float r = readDelayTail (DAP2B);
            previousRead = r;
            acc += r * (-kap);
        }
        // c.WriteAllPass(dap2b, kap);
        writeDelay (DAP2B, 0, acc);
        acc = acc * kap + previousRead;

        // c.Write(del2, 2.0f);
        writeDelay (DEL2, 0, acc);
        acc *= 2.0f;

        // c.Write(wet, 0.0f);
        wet = acc;
        // acc *= 0.0f;  // not needed

        right += (wet - right) * amount_;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        lpDecay1 = 0.0f;
        lpDecay2 = 0.0f;
        bwState  = 0.0f;
        writePtr = 0;
    }

private:
    // Delay line indices
    enum { AP1=0, AP2, AP3, AP4, DAP1A, DAP1B, DEL1, DAP2A, DAP2B, DEL2, NUM_DELAYS };

    struct DL { int base = 0; int length = 0; };

    // Read from tail (last sample) of delay line
    float readDelayTail (int idx) const
    {
        int pos = (writePtr + dl[idx].base + dl[idx].length) % bufferSize;
        if (pos < 0) pos += bufferSize;
        return buffer[static_cast<size_t> (pos)];
    }

    // Interpolated read at fractional offset
    float interpolateDelay (int idx, float offset) const
    {
        // Clamp to valid range
        float maxOff = static_cast<float> (dl[idx].length - 1);
        if (offset < 0.0f) offset = 0.0f;
        if (offset > maxOff) offset = maxOff;

        int intPart = static_cast<int> (offset);
        float frac  = offset - static_cast<float> (intPart);

        int posA = (writePtr + dl[idx].base + intPart) % bufferSize;
        int posB = (writePtr + dl[idx].base + intPart + 1) % bufferSize;
        if (posA < 0) posA += bufferSize;
        if (posB < 0) posB += bufferSize;

        float a = buffer[static_cast<size_t> (posA)];
        float b = buffer[static_cast<size_t> (posB)];
        return a + (b - a) * frac;
    }

    // Write at offset within delay line
    void writeDelay (int idx, int offset, float value)
    {
        int pos = (writePtr + dl[idx].base + offset) % bufferSize;
        if (pos < 0) pos += bufferSize;
        buffer[static_cast<size_t> (pos)] = value;
    }

    float sizeF() const
    {
        return sizeScale * static_cast<float> (sr / 32000.0);
    }

    static constexpr double PI = 3.14159265358979323846;

    double sr = 32000.0;
    std::vector<float> buffer;
    int bufferSize = 0;
    int writePtr   = 0;
    DL dl[NUM_DELAYS];

    double lfo1Phase = 0.0, lfo2Phase = 0.0;
    double lfo1Inc = 0.0,   lfo2Inc = 0.0;
    int    lfoCounter = 0;
    float  lfo1Val = 0.0f,  lfo2Val = 0.0f;

    float reverbTime   = 0.85f;
    float diffusion_   = 0.625f;
    float lp_          = 0.7f;
    float amount_      = 0.35f;
    float inputGain_   = 0.2f;
    float bandwidth_   = 0.9995f;
    float modRateScale  = 1.0f;
    float modDepthScale = 1.0f;
    float sizeScale     = 1.0f;

    float lpDecay1 = 0.0f;
    float lpDecay2 = 0.0f;
    float bwState  = 0.0f;
};

}  // namespace DSP
