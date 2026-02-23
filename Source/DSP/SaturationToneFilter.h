#pragma once

#include <cmath>

namespace DSP
{

/**
 * サチュレーション後のトーン制御。
 * 1 次ローパス / ハイパスのブレンドで実現。
 * sat_tone: -100% = ダーク（ローパス）、0% = フラット、+100% = ブライト（ハイパス強調）
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
     */
    void setTone (float tonePercent)
    {
        // -100%: fc = 1kHz ローパス
        //    0%: バイパス
        // +100%: fc = 8kHz ローパスの減衰 → ブライト強調
        tone = tonePercent * 0.01f;  // -1 to +1

        if (std::abs (tone) < 0.01f)
        {
            isActive = false;
            return;
        }
        isActive = true;

        float freq;
        if (tone < 0.0f)
            freq = 1000.0f + (1.0f + tone) * 7000.0f;  // 1kHz–8kHz
        else
            freq = 8000.0f - tone * 4000.0f;             // 4kHz–8kHz

        float w = 2.0f * 3.14159265f * freq / sr;
        lpCoeff = w / (1.0f + w);
    }

    float process (float input)
    {
        if (!isActive)
            return input;

        // 1 次ローパスフィルタ
        lpState += lpCoeff * (input - lpState);

        if (tone < 0.0f)
        {
            // ダーク: ローパスフィルタ出力をブレンド
            float blend = -tone;
            return (1.0f - blend) * input + blend * lpState;
        }
        else
        {
            // ブライト: ハイパス成分をブースト
            float hp = input - lpState;
            return input + tone * hp * 0.5f;
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
