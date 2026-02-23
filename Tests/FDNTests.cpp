#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/DSP/FDNReverb.h"
#include "../Source/DSP/FeedbackMatrix.h"

//==============================================================================
class FDNStabilityTests : public juce::UnitTest
{
public:
    FDNStabilityTests() : juce::UnitTest ("FDN Stability Tests") {}

    void runTest() override
    {
        beginTest ("Householder matrix is energy-preserving");
        {
            DSP::FeedbackMatrix matrix;
            std::array<float, 8> input = { 1.0f, 0.0f, 0.0f, 0.0f,
                                            0.0f, 0.0f, 0.0f, 0.0f };
            std::array<float, 8> output;
            matrix.process (input, output);

            float inputEnergy = 0.0f;
            for (auto v : input) inputEnergy += v * v;

            float outputEnergy = 0.0f;
            for (auto v : output) outputEnergy += v * v;

            expectWithinAbsoluteError (outputEnergy, inputEnergy, 0.01f,
                "Householder matrix should preserve energy");
        }

        beginTest ("Householder matrix preserves energy for arbitrary input");
        {
            DSP::FeedbackMatrix matrix;
            std::array<float, 8> input = { 0.3f, -0.5f, 0.1f, 0.7f,
                                            -0.2f, 0.4f, -0.6f, 0.8f };
            std::array<float, 8> output;
            matrix.process (input, output);

            float inputEnergy = 0.0f;
            for (auto v : input) inputEnergy += v * v;

            float outputEnergy = 0.0f;
            for (auto v : output) outputEnergy += v * v;

            expectWithinAbsoluteError (outputEnergy, inputEnergy, 0.02f,
                "Householder matrix should preserve energy for arbitrary input");
        }

        beginTest ("FDN output decays over time with finite RT60");
        {
            DSP::FDNReverb fdn;
            fdn.prepare (44100.0, 512);
            fdn.setParameters (0.6f, 1.0f, 0.5f, 65.0f, 80.0f,
                               0.0f, 0.5f,
                               0.0f, 6.0f, 1, 0.0f, 0.0f);

            // インパルスを入力
            float outL, outR;
            fdn.processSample (1.0f, 1.0f, outL, outR);

            // 5 秒分処理して最終振幅を測定
            float lastMaxAmplitude = 0.0f;
            for (int i = 0; i < 44100 * 5; ++i)
            {
                fdn.processSample (0.0f, 0.0f, outL, outR);
                if (i > 44100 * 4)
                    lastMaxAmplitude = std::max (lastMaxAmplitude,
                        std::max (std::abs (outL), std::abs (outR)));
            }

            // RT60=1s なので 5 秒後は -300dB 相当 → ほぼゼロ
            expect (lastMaxAmplitude < 0.001f,
                "FDN output should decay to near zero after 5x RT60, got "
                + juce::String (lastMaxAmplitude));
        }

        beginTest ("FDN does not blow up (stability test)");
        {
            DSP::FDNReverb fdn;
            fdn.prepare (44100.0, 512);
            fdn.setParameters (1.0f, 5.0f, 3.0f, 65.0f, 80.0f,
                               15.0f, 0.5f,
                               50.0f, 12.0f, 1, 0.0f, 20.0f);

            // ランダムな入力を 10 秒分処理
            uint32_t rng = 0x42424242u;
            float maxOutput = 0.0f;
            bool hasNaN = false;
            bool hasInf = false;

            for (int i = 0; i < 44100 * 10; ++i)
            {
                rng = rng * 1664525u + 1013904223u;
                float noise = (static_cast<float> (rng) / static_cast<float> (0xFFFFFFFFu)) * 2.0f - 1.0f;
                float inputSample = (i < 44100) ? noise * 0.5f : 0.0f;

                float outL, outR;
                fdn.processSample (inputSample, inputSample, outL, outR);

                if (std::isnan (outL) || std::isnan (outR)) hasNaN = true;
                if (std::isinf (outL) || std::isinf (outR)) hasInf = true;
                maxOutput = std::max (maxOutput,
                    std::max (std::abs (outL), std::abs (outR)));
            }

            expect (!hasNaN, "FDN output should not contain NaN");
            expect (!hasInf, "FDN output should not contain Inf");
            expect (maxOutput < 10.0f,
                "FDN output should not blow up, max was " + juce::String (maxOutput));
        }

        beginTest ("FDN produces output (not silent)");
        {
            DSP::FDNReverb fdn;
            fdn.prepare (44100.0, 512);
            fdn.setParameters (0.6f, 1.8f, 0.9f, 65.0f, 80.0f,
                               15.0f, 0.5f,
                               0.0f, 6.0f, 1, 0.0f, 0.0f);

            // インパルス入力
            float outL, outR;
            fdn.processSample (1.0f, 1.0f, outL, outR);

            // 1000 サンプル後に出力があるか確認
            float totalEnergy = 0.0f;
            for (int i = 0; i < 1000; ++i)
            {
                fdn.processSample (0.0f, 0.0f, outL, outR);
                totalEnergy += outL * outL + outR * outR;
            }

            expect (totalEnergy > 1.0e-6f,
                "FDN should produce audible output after impulse");
        }

        beginTest ("FDN reset clears all state");
        {
            DSP::FDNReverb fdn;
            fdn.prepare (44100.0, 512);
            fdn.setParameters (0.6f, 1.8f, 0.9f, 65.0f, 80.0f,
                               15.0f, 0.5f,
                               0.0f, 6.0f, 1, 0.0f, 0.0f);

            // 信号を処理
            float outL, outR;
            for (int i = 0; i < 44100; ++i)
            {
                float in = (i < 100) ? 1.0f : 0.0f;
                fdn.processSample (in, in, outL, outR);
            }

            // リセット
            fdn.reset();

            // リセット後は無音のはず
            float totalEnergy = 0.0f;
            for (int i = 0; i < 1000; ++i)
            {
                fdn.processSample (0.0f, 0.0f, outL, outR);
                totalEnergy += outL * outL + outR * outR;
            }

            expect (totalEnergy < 1.0e-10f,
                "FDN should be silent after reset");
        }
    }
};

static FDNStabilityTests fdnStabilityTests;
