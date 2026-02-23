#pragma once

#include "DSP/VelvetNoise.h"
#include <vector>

namespace DSP
{

/**
 * OVN 初期反射（第 1 層）。
 * スパース FIR で弦楽器のトランジェントを保持。
 * 線形処理のためオーバーサンプリング不要。
 */
class EarlyReflections
{
public:
    EarlyReflections() = default;

    void prepare (double sampleRate, int maxBlockSize, uint32_t seed)
    {
        sr = sampleRate;
        inputBuffer.resize (static_cast<size_t> (maxBlockSize + 2048), 0.0f);
        writePos = 0;

        // OVN パルス列を生成（~30ms、2000 pulses/s）
        ovn.generate (sampleRate, 30.0f, 2000.0f, seed);
    }

    /**
     * 初期反射を処理。
     * @param input 入力サンプル配列
     * @param output 出力サンプル配列
     * @param numSamples サンプル数
     * @param gain ゲイン（dB から線形に変換済み）
     */
    void process (const float* input, float* output, int numSamples, float gain)
    {
        ovn.convolve (input, output, numSamples, gain);
    }

    void reset()
    {
        std::fill (inputBuffer.begin(), inputBuffer.end(), 0.0f);
        writePos = 0;
    }

private:
    VelvetNoise ovn;
    double sr = 44100.0;
    std::vector<float> inputBuffer;
    int writePos = 0;
};

}  // namespace DSP
