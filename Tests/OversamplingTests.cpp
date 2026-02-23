#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "../Source/PluginProcessor.h"
#include "../Source/DSP/OversamplingManager.h"

//==============================================================================
class OversamplingTests : public juce::UnitTest
{
public:
    OversamplingTests() : juce::UnitTest ("Oversampling Tests") {}

    void runTest() override
    {
        beginTest ("OS Off does not crash");
        {
            processWithOversamplingFactor (0, 44100.0, 512);
        }

        beginTest ("OS 2x does not crash");
        {
            processWithOversamplingFactor (1, 44100.0, 512);
        }

        beginTest ("OS 4x does not crash");
        {
            processWithOversamplingFactor (2, 44100.0, 512);
        }

        beginTest ("OS latency is reported correctly");
        {
            DSP::OversamplingManager osm;

            osm.prepare (2, 0, 44100.0, 512);
            expectEquals (osm.getLatencyInSamples(), 0.0f,
                "Off should have zero latency");

            osm.prepare (2, 1, 44100.0, 512);
            expect (osm.getLatencyInSamples() > 0.0f,
                "2x should have non-zero latency");

            osm.prepare (2, 2, 44100.0, 512);
            expect (osm.getLatencyInSamples() > 0.0f,
                "4x should have non-zero latency");
        }

        beginTest ("OS rate calculation is correct");
        {
            DSP::OversamplingManager osm;

            osm.prepare (2, 0, 44100.0, 512);
            expectWithinAbsoluteError (
                static_cast<float> (osm.getOversampledRate (44100.0)),
                44100.0f, 0.1f, "Off: rate should be 44100");

            osm.prepare (2, 1, 44100.0, 512);
            expectWithinAbsoluteError (
                static_cast<float> (osm.getOversampledRate (44100.0)),
                88200.0f, 0.1f, "2x: rate should be 88200");

            osm.prepare (2, 2, 44100.0, 512);
            expectWithinAbsoluteError (
                static_cast<float> (osm.getOversampledRate (44100.0)),
                176400.0f, 0.1f, "4x: rate should be 176400");
        }

        // 9 パターン（SR × OS）全組合せ
        const std::array<double, 3> sampleRates = { 44100.0, 48000.0, 96000.0 };
        const std::array<int, 3> osFactors = { 0, 1, 2 };

        for (auto sr : sampleRates)
        {
            for (auto osFactor : osFactors)
            {
                juce::String testName = "SR=" + juce::String (sr)
                                      + " OS=" + juce::String (osFactor)
                                      + " does not crash";
                beginTest (testName);
                {
                    processWithOversamplingFactor (osFactor, sr, 256);
                }
            }
        }

        beginTest ("OS factor change does not crash");
        {
            VelvetUnderDronProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;

            // Off → 2x → 4x → Off の切替
            std::array<int, 4> factors = { 0, 1, 2, 0 };
            for (auto f : factors)
            {
                auto* param = processor.apvts.getParameter (Parameters::OVERSAMPLING);
                if (param != nullptr)
                    param->setValueNotifyingHost (
                        static_cast<float> (f) / 2.0f);

                buffer.clear();
                buffer.getWritePointer (0)[0] = 0.5f;
                buffer.getWritePointer (1)[0] = 0.5f;
                processor.processBlock (buffer, midi);

                // NaN/Inf チェック
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* data = buffer.getReadPointer (ch);
                    for (int i = 0; i < 512; ++i)
                    {
                        expect (!std::isnan (data[i]),
                            "Output should not be NaN after OS change to " + juce::String (f));
                        expect (!std::isinf (data[i]),
                            "Output should not be Inf after OS change to " + juce::String (f));
                    }
                }
            }
        }
    }

private:
    void processWithOversamplingFactor (int factor, double sampleRate, int blockSize)
    {
        VelvetUnderDronProcessor processor;

        // OS パラメータを設定してから prepare
        auto* param = processor.apvts.getParameter (Parameters::OVERSAMPLING);
        if (param != nullptr)
            param->setValueNotifyingHost (static_cast<float> (factor) / 2.0f);

        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        // インパルスを処理
        buffer.clear();
        buffer.getWritePointer (0)[0] = 1.0f;
        buffer.getWritePointer (1)[0] = 1.0f;
        processor.processBlock (buffer, midi);

        // 数ブロック追加処理
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
                expect (!std::isnan (data[i]), "Output should not be NaN");
                expect (!std::isinf (data[i]), "Output should not be Inf");
            }
        }
    }
};

static OversamplingTests oversamplingTests;
