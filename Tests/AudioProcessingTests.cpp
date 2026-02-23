#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/PluginProcessor.h"
#include "../Source/Parameters.h"
#include <cmath>

//==============================================================================
class AudioProcessingTests : public juce::UnitTest
{
public:
    AudioProcessingTests() : juce::UnitTest ("Audio Processing Tests") {}

    void runTest() override
    {
        beginTest ("Silent input produces silent output when dry_wet=0%");
        {
            WetStringReverbProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            // dry_wet=0% に設定
            auto* param = processor.apvts.getParameter (Parameters::DRY_WET);
            if (param != nullptr)
                param->setValueNotifyingHost (param->convertTo0to1 (0.0f));

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            buffer.clear();

            // 数ブロック処理してテール排出
            for (int b = 0; b < 20; ++b)
            {
                buffer.clear();
                processor.processBlock (buffer, midi);
            }

            float maxSample = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = buffer.getReadPointer (ch);
                for (int i = 0; i < 512; ++i)
                    maxSample = std::max (maxSample, std::abs (data[i]));
            }

            expect (maxSample < 0.0001f,
                "Silent input with dry_wet=0% should produce silence, max was "
                + juce::String (maxSample));
        }

        // サンプルレートテスト
        double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
        for (auto sr : sampleRates)
        {
            juce::String testName = "Processes without crash at SR="
                                  + juce::String (sr);
            beginTest (testName);
            {
                processAtSampleRate (sr, 512);
            }
        }

        // バッファサイズテスト
        int bufferSizes[] = { 64, 128, 256, 512, 1024 };
        for (auto bs : bufferSizes)
        {
            juce::String testName = "Processes without crash at bufferSize="
                                  + juce::String (bs);
            beginTest (testName);
            {
                processAtSampleRate (44100.0, bs);
            }
        }

        beginTest ("Output is not all zeros with wet signal");
        {
            WetStringReverbProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            // dry_wet=100%
            auto* param = processor.apvts.getParameter (Parameters::DRY_WET);
            if (param != nullptr)
                param->setValueNotifyingHost (param->convertTo0to1 (100.0f));

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;

            // インパルスを入力
            buffer.clear();
            buffer.getWritePointer (0)[0] = 1.0f;
            buffer.getWritePointer (1)[0] = 1.0f;
            processor.processBlock (buffer, midi);

            // 数ブロック後にまだ出力があるはず（リバーブテール）
            float totalEnergy = 0.0f;
            for (int b = 0; b < 5; ++b)
            {
                buffer.clear();
                processor.processBlock (buffer, midi);
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* data = buffer.getReadPointer (ch);
                    for (int i = 0; i < 512; ++i)
                        totalEnergy += data[i] * data[i];
                }
            }

            expect (totalEnergy > 1.0e-10f,
                "Wet signal should produce non-zero output (reverb tail)");
        }

        beginTest ("No NaN or Inf in output");
        {
            WetStringReverbProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;

            // ランダム入力
            uint32_t rng = 0x13371337u;
            for (int b = 0; b < 50; ++b)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* data = buffer.getWritePointer (ch);
                    for (int i = 0; i < 512; ++i)
                    {
                        rng = rng * 1664525u + 1013904223u;
                        data[i] = (static_cast<float> (rng) / static_cast<float> (0xFFFFFFFFu))
                                * 2.0f - 1.0f;
                    }
                }
                processor.processBlock (buffer, midi);

                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* data = buffer.getReadPointer (ch);
                    for (int i = 0; i < 512; ++i)
                    {
                        expect (!std::isnan (data[i]),
                            "Output should not contain NaN at block " + juce::String (b));
                        expect (!std::isinf (data[i]),
                            "Output should not contain Inf at block " + juce::String (b));
                    }
                }
            }
        }

        beginTest ("Extreme parameter values do not crash");
        {
            WetStringReverbProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            // 全パラメータを最大値に設定
            const char* floatParams[] = {
                Parameters::DRY_WET, Parameters::PRE_DELAY_MS,
                Parameters::EARLY_LEVEL_DB, Parameters::LATE_LEVEL_DB,
                Parameters::ROOM_SIZE, Parameters::STEREO_WIDTH,
                Parameters::LOW_RT60_S, Parameters::HIGH_RT60_S,
                Parameters::HF_DAMPING, Parameters::DIFFUSION,
                Parameters::DECAY_SHAPE, Parameters::SAT_AMOUNT,
                Parameters::SAT_DRIVE_DB, Parameters::SAT_TONE,
                Parameters::SAT_ASYMMETRY, Parameters::MOD_DEPTH,
                Parameters::MOD_RATE_HZ
            };

            for (auto* id : floatParams)
            {
                auto* p = processor.apvts.getParameter (id);
                if (p != nullptr)
                    p->setValueNotifyingHost (1.0f);  // 最大値
            }

            // OS=4x
            auto* osP = processor.apvts.getParameter (Parameters::OVERSAMPLING);
            if (osP != nullptr)
                osP->setValueNotifyingHost (1.0f);

            // Sat Type=Tube
            auto* stP = processor.apvts.getParameter (Parameters::SAT_TYPE);
            if (stP != nullptr)
                stP->setValueNotifyingHost (1.0f);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            buffer.clear();
            buffer.getWritePointer (0)[0] = 1.0f;
            buffer.getWritePointer (1)[0] = 1.0f;

            // クラッシュしなければ OK
            for (int b = 0; b < 20; ++b)
            {
                processor.processBlock (buffer, midi);
                buffer.clear();
            }

            expect (true, "Extreme parameters did not crash");
        }

        beginTest ("Mono input is handled correctly");
        {
            WetStringReverbProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            // 1ch バッファでも動作するか確認
            // 注: 実際には JUCE がステレオバスで呼び出すが、
            // 内部で 1ch 入力のハンドリングをテスト
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            buffer.clear();
            buffer.getWritePointer (0)[0] = 1.0f;
            // ch1 はゼロのまま

            processor.processBlock (buffer, midi);

            // NaN/Inf がなければ OK
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = buffer.getReadPointer (ch);
                for (int i = 0; i < 512; ++i)
                {
                    expect (!std::isnan (data[i]), "No NaN with mono-like input");
                    expect (!std::isinf (data[i]), "No Inf with mono-like input");
                }
            }
        }
    }

private:
    void processAtSampleRate (double sr, int blockSize)
    {
        WetStringReverbProcessor processor;
        processor.prepareToPlay (sr, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        // インパルス入力
        buffer.clear();
        buffer.getWritePointer (0)[0] = 1.0f;
        buffer.getWritePointer (1)[0] = 1.0f;
        processor.processBlock (buffer, midi);

        // 追加ブロック
        for (int b = 0; b < 10; ++b)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
        }

        // NaN/Inf チェック
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = buffer.getReadPointer (ch);
            for (int i = 0; i < blockSize; ++i)
            {
                expect (!std::isnan (data[i]),
                    "No NaN at SR=" + juce::String (sr)
                    + " BS=" + juce::String (blockSize));
                expect (!std::isinf (data[i]),
                    "No Inf at SR=" + juce::String (sr)
                    + " BS=" + juce::String (blockSize));
            }
        }
    }
};

static AudioProcessingTests audioProcessingTests;
