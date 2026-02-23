#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace DSP
{

/**
 * Multi-channel allpass diffuser (Signalsmith 2021 design).
 *
 * Each "step" consists of:
 *   1. Per-channel delay (different lengths)
 *   2. Channel shuffle + polarity flip
 *   3. Hadamard mixing matrix (energy-preserving)
 *
 * Cascading NUM_STEPS of these produces N^NUM_STEPS echoes
 * from a single input pulse, giving immediate high density.
 *
 * Tuning change (v1.1): delay ranges shortened from 20/40/80/160ms
 * to 5/10/20/40ms for a more natural pre-reverb onset that doesn't
 * smear transients excessively.
 */
class Diffuser
{
public:
    static constexpr int NUM_CHANNELS = 8;
    static constexpr int NUM_STEPS    = 4;

    Diffuser() = default;

    void prepare (double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;

        // Shortened delay ranges per step: 5ms, 10ms, 20ms, 40ms
        // Total diffusion spread ≈ 75ms (was 300ms), giving a tighter
        // pre-reverb cluster that preserves transient clarity while
        // still achieving 8^4 = 4096 echoes for high density.
        const float stepDurationsMs[NUM_STEPS] = { 5.0f, 10.0f, 20.0f, 40.0f };

        uint32_t rng = 0xBAADF00Du;

        for (int step = 0; step < NUM_STEPS; ++step)
        {
            float maxDelaySamples = stepDurationsMs[step] * 0.001f
                                  * static_cast<float> (sampleRate);

            for (int ch = 0; ch < NUM_CHANNELS; ++ch)
            {
                // Distribute delays across sub-ranges for evenness
                float lo = maxDelaySamples * static_cast<float> (ch)
                         / static_cast<float> (NUM_CHANNELS);
                float hi = maxDelaySamples * static_cast<float> (ch + 1)
                         / static_cast<float> (NUM_CHANNELS);

                rng = rng * 1664525u + 1013904223u;
                float t = static_cast<float> (rng & 0xFFFFu) / 65535.0f;
                int delaySamples = std::max (1, static_cast<int> (lo + t * (hi - lo)));

                steps[step].delaySamples[ch] = delaySamples;

                int bufSize = delaySamples + 1;
                steps[step].buffers[ch].assign (static_cast<size_t> (bufSize), 0.0f);
                steps[step].bufferSize[ch] = bufSize;
                steps[step].writePos[ch] = 0;
            }

            // Generate per-step shuffle order and polarity flips
            // Simple: rotate channels by (step+1) and flip based on RNG
            for (int ch = 0; ch < NUM_CHANNELS; ++ch)
            {
                steps[step].shuffleOrder[ch] = (ch + step + 1) % NUM_CHANNELS;
                rng = rng * 1664525u + 1013904223u;
                steps[step].flipSign[ch] = (rng & 0x80000000u) ? -1.0f : 1.0f;
            }
        }

        // Build Hadamard matrix (same as FeedbackMatrix but without sign arrays)
        float h[NUM_CHANNELS][NUM_CHANNELS];
        h[0][0] = 1.0f;
        int size = 1;
        while (size < NUM_CHANNELS)
        {
            for (int i = 0; i < size; ++i)
            {
                for (int j = 0; j < size; ++j)
                {
                    float val = h[i][j];
                    h[i][j + size]        =  val;
                    h[i + size][j]        =  val;
                    h[i + size][j + size] = -val;
                }
            }
            size *= 2;
        }
        float norm = 1.0f / std::sqrt (static_cast<float> (NUM_CHANNELS));
        for (int i = 0; i < NUM_CHANNELS; ++i)
            for (int j = 0; j < NUM_CHANNELS; ++j)
                hadamard[i][j] = h[i][j] * norm;
    }

    /**
     * Process a single sample through all diffusion steps.
     * Input: 8-channel array, Output: 8-channel array.
     */
    void processSample (const std::array<float, NUM_CHANNELS>& input,
                        std::array<float, NUM_CHANNELS>& output)
    {
        std::array<float, NUM_CHANNELS> current = input;

        for (int step = 0; step < NUM_STEPS; ++step)
        {
            auto& s = steps[step];

            // 1. Per-channel delay
            std::array<float, NUM_CHANNELS> delayed;
            for (int ch = 0; ch < NUM_CHANNELS; ++ch)
            {
                // Write current sample
                s.buffers[ch][static_cast<size_t> (s.writePos[ch])] = current[ch];

                // Read delayed sample
                int readPos = (s.writePos[ch] - s.delaySamples[ch]
                             + s.bufferSize[ch] * 2) % s.bufferSize[ch];
                delayed[ch] = s.buffers[ch][static_cast<size_t> (readPos)];

                // Advance write pointer
                s.writePos[ch] = (s.writePos[ch] + 1) % s.bufferSize[ch];
            }

            // 2. Shuffle + polarity flip
            std::array<float, NUM_CHANNELS> shuffled;
            for (int ch = 0; ch < NUM_CHANNELS; ++ch)
                shuffled[ch] = s.flipSign[ch] * delayed[s.shuffleOrder[ch]];

            // 3. Hadamard mixing
            for (int i = 0; i < NUM_CHANNELS; ++i)
            {
                float sum = 0.0f;
                for (int j = 0; j < NUM_CHANNELS; ++j)
                    sum += hadamard[i][j] * shuffled[j];
                current[i] = sum;
            }
        }

        output = current;
    }

    void reset()
    {
        for (int step = 0; step < NUM_STEPS; ++step)
            for (int ch = 0; ch < NUM_CHANNELS; ++ch)
            {
                std::fill (steps[step].buffers[ch].begin(),
                           steps[step].buffers[ch].end(), 0.0f);
                steps[step].writePos[ch] = 0;
            }
    }

private:
    struct DiffusionStep
    {
        std::array<int, NUM_CHANNELS> delaySamples {};
        std::array<std::vector<float>, NUM_CHANNELS> buffers;
        std::array<int, NUM_CHANNELS> bufferSize {};
        std::array<int, NUM_CHANNELS> writePos {};
        std::array<int, NUM_CHANNELS> shuffleOrder {};
        std::array<float, NUM_CHANNELS> flipSign {};
    };

    double sr = 44100.0;
    std::array<DiffusionStep, NUM_STEPS> steps;
    float hadamard[NUM_CHANNELS][NUM_CHANNELS] {};
};

}  // namespace DSP
