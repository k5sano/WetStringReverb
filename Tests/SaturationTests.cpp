#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/DSP/Saturation.h"
#include <cmath>
#include <array>

//==============================================================================
class SaturationTests : public juce::UnitTest
{
public:
    SaturationTests() : juce::UnitTest ("Saturation Tests") {}

    void runTest() override
    {
        beginTest ("Amount=0% is completely linear (bypass)");
        {
            DSP::Saturation sat;
            sat.prepare (44100.0);
            sat.setParameters (0.0f, 12.0f, 1, 50.0f);  // amount=0

            float maxDiff = 0.0f;
            for (int i = 0; i < 10000; ++i)
            {
                float input = (static_cast<float> (i) / 10000.0f) * 2.0f - 1.0f;
                float output = sat.process (input);
                maxDiff = std::max (maxDiff, std::abs (output - input));
            }

            float diffDb = 20.0f * std::log10 (maxDiff + 1.0e-20f);
            expect (diffDb < -120.0f,
                "Amount=0% diff should be < -120dB, got " + juce::String (diffDb) + "dB");
        }

        beginTest ("Drive=24dB, Amount=100% output is bounded to +-1.0");
        {
            // Warm (tanh) タイプ
            DSP::Saturation sat;
            sat.prepare (44100.0);
            sat.setParameters (100.0f, 24.0f, 1, 0.0f);  // Warm, max drive

            float maxOutput = 0.0f;
            for (int i = 0; i < 10000; ++i)
            {
                float input = (static_cast<float> (i) / 10000.0f) * 2.0f - 1.0f;
                float output = sat.process (input);
                maxOutput = std::max (maxOutput, std::abs (output));
            }

            expect (maxOutput <= 1.05f,
                "Warm saturation output should be bounded, max was "
                + juce::String (maxOutput));
        }

        beginTest ("Drive=24dB, Amount=100% Soft type output is bounded");
        {
            DSP::Saturation sat;
            sat.prepare (44100.0);
            sat.setParameters (100.0f, 24.0f, 0, 0.0f);  // Soft

            float maxOutput = 0.0f;
            for (int i = 0; i < 10000; ++i)
            {
                float input = (static_cast<float> (i) / 10000.0f) * 2.0f - 1.0f;
                float output = sat.process (input);
                maxOutput = std::max (maxOutput, std::abs (output));
            }

            expect (maxOutput <= 1.1f,
                "Soft saturation output should be bounded, max was "
                + juce::String (maxOutput));
        }

        beginTest ("Drive=24dB, Amount=100% Tape type output is bounded");
        {
            DSP::Saturation sat;
            sat.prepare (44100.0);
            sat.setParameters (100.0f, 24.0f, 2, 0.0f);  // Tape

            float maxOutput = 0.0f;
            for (int i = 0; i < 10000; ++i)
            {
                float input = (static_cast<float> (i) / 10000.0f) * 2.0f - 1.0f;
                float output = sat.process (input);
                maxOutput = std::max (maxOutput, std::abs (output));
            }

            expect (maxOutput <= 1.3f,
                "Tape saturation output should be bounded, max was "
                + juce::String (maxOutput));
        }

        beginTest ("Drive=24dB, Amount=100% Tube type output is bounded");
        {
            DSP::Saturation sat;
            sat.prepare (44100.0);
            sat.setParameters (100.0f, 24.0f, 3, 0.0f);  // Tube

            float maxOutput = 0.0f;
            for (int i = 0; i < 10000; ++i)
            {
                float input = (static_cast<float> (i) / 10000.0f) * 2.0f - 1.0f;
                float output = sat.process (input);
                maxOutput = std::max (maxOutput, std::abs (output));
            }

            expect (maxOutput <= 1.1f,
                "Tube saturation output should be bounded, max was "
                + juce::String (maxOutput));
        }

        beginTest ("Warm type generates 3rd harmonic (THD > -60dB)");
        {
            // 正弦波入力で THD を測定
            DSP::Saturation sat;
            double sr = 44100.0;
            sat.prepare (sr);
            sat.setParameters (100.0f, 12.0f, 1, 0.0f);  // Warm, 12dB drive

            int N = 4096;
            float freq = 1000.0f;
            std::vector<float> output (static_cast<size_t> (N));

            for (int i = 0; i < N; ++i)
            {
                float input = 0.5f * std::sin (2.0f * 3.14159265f * freq
                    * static_cast<float> (i) / static_cast<float> (sr));
                output[static_cast<size_t> (i)] = sat.process (input);
            }

            // 簡易 DFT で基本波と第 3 次倍音のパワーを測定
            float fund = goertzel (output.data(), N, freq, static_cast<float> (sr));
            float harm3 = goertzel (output.data(), N, freq * 3.0f, static_cast<float> (sr));

            float thd3Db = 20.0f * std::log10 (harm3 / (fund + 1.0e-20f));
            expect (thd3Db > -60.0f,
                "Warm type should have 3rd harmonic > -60dB relative to fundamental, got "
                + juce::String (thd3Db) + "dB");
        }

        beginTest ("Asymmetry=50% increases 2nd harmonic by >10dB");
        {
            double sr = 44100.0;
            float freq = 1000.0f;
            int N = 4096;

            // 対称 (asymmetry=0%)
            DSP::Saturation satSym;
            satSym.prepare (sr);
            satSym.setParameters (100.0f, 12.0f, 1, 0.0f);

            std::vector<float> outSym (static_cast<size_t> (N));
            for (int i = 0; i < N; ++i)
            {
                float input = 0.5f * std::sin (2.0f * 3.14159265f * freq
                    * static_cast<float> (i) / static_cast<float> (sr));
                outSym[static_cast<size_t> (i)] = satSym.process (input);
            }

            // 非対称 (asymmetry=50%)
            DSP::Saturation satAsym;
            satAsym.prepare (sr);
            satAsym.setParameters (100.0f, 12.0f, 1, 50.0f);

            std::vector<float> outAsym (static_cast<size_t> (N));
            for (int i = 0; i < N; ++i)
            {
                float input = 0.5f * std::sin (2.0f * 3.14159265f * freq
                    * static_cast<float> (i) / static_cast<float> (sr));
                outAsym[static_cast<size_t> (i)] = satAsym.process (input);
            }

            float harm2Sym = goertzel (outSym.data(), N, freq * 2.0f, static_cast<float> (sr));
            float harm2Asym = goertzel (outAsym.data(), N, freq * 2.0f, static_cast<float> (sr));

            float diffDb = 20.0f * std::log10 ((harm2Asym + 1.0e-20f)
                                              / (harm2Sym + 1.0e-20f));
            expect (diffDb > 10.0f,
                "Asymmetry=50% should increase 2nd harmonic by >10dB, got "
                + juce::String (diffDb) + "dB");
        }

        beginTest ("Saturation reset clears DC blocker state");
        {
            DSP::Saturation sat;
            sat.prepare (44100.0);
            sat.setParameters (100.0f, 12.0f, 1, 50.0f);

            // 大量の信号を処理（DC 蓄積）
            for (int i = 0; i < 10000; ++i)
                sat.process (0.8f);

            sat.reset();

            // リセット後、ゼロ入力でゼロ出力
            float output = sat.process (0.0f);
            expectWithinAbsoluteError (output, 0.0f, 0.001f,
                "After reset, zero input should produce near-zero output");
        }
    }

private:
    /** Goertzel アルゴリズムで特定周波数の振幅を計算 */
    float goertzel (const float* data, int N, float targetFreq, float sampleRate) const
    {
        float k = targetFreq * static_cast<float> (N) / sampleRate;
        float w = 2.0f * 3.14159265f * k / static_cast<float> (N);
        float coeff = 2.0f * std::cos (w);

        float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            s0 = data[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        return std::sqrt (std::max (0.0f, power)) / static_cast<float> (N) * 2.0f;
    }
};

static SaturationTests saturationTests;
