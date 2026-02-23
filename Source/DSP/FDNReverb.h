#pragma once

#include "DSP/DelayLine.h"
#include "DSP/FeedbackMatrix.h"
#include "DSP/AttenuationFilter.h"
#include "DSP/Saturation.h"
#include "DSP/SaturationToneFilter.h"
#include <array>
#include <cmath>

namespace DSP
{

/**
 * 8 チャンネル Feedback Delay Network（第 2 層）。
 * オーバーサンプルされたサンプルレートで動作。
 *
 * 参考:
 *   Dal Santo et al., "Optimizing Tiny Colorless FDNs", EURASIP JASM (2025)
 *   Schlecht & Habets, "Scattering in FDNs", IEEE/ACM TASLP (2020)
 */
class FDNReverb
{
public:
    static constexpr int NUM_CHANNELS = 8;

    // 素数ベースの基本ディレイ長（@44.1kHz）
    static constexpr std::array<int, NUM_CHANNELS> BASE_DELAYS = {
        443, 557, 661, 769, 883, 1013, 1151, 1277
    };

    FDNReverb() = default;

    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;

        int maxDelay = static_cast<int> (1277 * 2.0 * (sampleRate / 44100.0)) + 64;
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            delayLines[i].prepare (maxDelay);
        }

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
                        float satTone, float satAsymmetry)
    {
        // ディレイ長の更新
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            float delayLength = static_cast<float> (BASE_DELAYS[i]) * roomSize
                              * static_cast<float> (sr / 44100.0);
            currentDelays[i] = delayLength;
            delayLines[i].setDelay (delayLength);
        }

        // クロスオーバー周波数（hf_damping 0-100% → 1kHz-8kHz）
        float crossoverHz = 1000.0f + (hfDamping * 0.01f) * 7000.0f;

        // 周波数依存減衰フィルタの更新
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            float delaySec = currentDelays[i] / static_cast<float> (sr);
            float gLow = std::pow (10.0f, -3.0f * delaySec / (lowRT60 + 1.0e-6f));
            float gHigh = std::pow (10.0f, -3.0f * delaySec / (highRT60 + 1.0e-6f));
            attenuationFilters[i].setCoefficients (gLow, gHigh, crossoverHz,
                                                    static_cast<float> (sr));
        }

        // サチュレーション設定
        for (auto& sat : saturators)
            sat.setParameters (satAmount, satDrive, satType, satAsymmetry);

        // トーンフィルタ
        for (auto& tf : toneFilters)
            tf.setTone (satTone);

        // モジュレーション
        currentModDepth = modDepth * 0.01f;
        currentModRate = modRate;
        maxModSamples = 4.0f;  // 最大 4 サンプル変調
    }

    /**
     * ステレオ入力を受け取り、8ch FDN で処理してステレオ出力を返す。
     */
    void processSample (float inputL, float inputR, float& outputL, float& outputR)
    {
        // ディレイラインから読み取り
        std::array<float, NUM_CHANNELS> delayOutputs;
        for (int i = 0; i < NUM_CHANNELS; ++i)
            delayOutputs[i] = delayLines[i].read();

        // 出力の計算（FDN 出力ゲイン）
        // L チャンネル: 偶数インデックス, R チャンネル: 奇数インデックス
        outputL = 0.0f;
        outputR = 0.0f;
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            if (i % 2 == 0)
                outputL += delayOutputs[i];
            else
                outputR += delayOutputs[i];
        }
        outputL /= static_cast<float> (NUM_CHANNELS / 2);
        outputR /= static_cast<float> (NUM_CHANNELS / 2);

        // フィードバック行列
        std::array<float, NUM_CHANNELS> matrixOutput;
        feedbackMatrix.process (delayOutputs, matrixOutput);

        // サチュレーション → トーンフィルタ → 減衰フィルタ
        std::array<float, NUM_CHANNELS> processed;
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            float sat = saturators[i].process (matrixOutput[i]);
            float toned = toneFilters[i].process (sat);
            processed[i] = attenuationFilters[i].process (toned);
        }

        // 変調 + ディレイライン書き込み
        float lfoInc = 2.0f * 3.14159265f * currentModRate / static_cast<float> (sr);
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            // 変調
            float phaseOffset = 2.0f * 3.14159265f * static_cast<float> (i)
                              / static_cast<float> (NUM_CHANNELS);
            float mod = currentModDepth * maxModSamples
                      * std::sin (static_cast<float> (lfoPhase) + phaseOffset);
            delayLines[i].setDelay (currentDelays[i] + mod);

            // 入力注入 + フィードバック書き込み
            float inputSample = (i % 2 == 0) ? inputL : inputR;
            delayLines[i].write (inputSample + processed[i]);
        }

        lfoPhase += static_cast<double> (lfoInc);
        if (lfoPhase > 2.0 * 3.14159265358979323846)
            lfoPhase -= 2.0 * 3.14159265358979323846;
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
    float currentModDepth = 0.0f;
    float currentModRate = 0.5f;
    float maxModSamples = 4.0f;
    double lfoPhase = 0.0;
};

}  // namespace DSP
