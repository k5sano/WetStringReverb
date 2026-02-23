#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/PluginProcessor.h"
#include "../Source/Parameters.h"
#include <cmath>
#include <array>

//==============================================================================
class AudioProcessingTests : public juce::UnitTest
{
public:
    AudioProcessingTests() : juce::UnitTest ("Audio Processing Tests") {}

    void runTest() override
    {
        beginTest ("Silent input with mix=0 produces silence");
        {
            WetStringReverbProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            auto* param = processor.apvts.getParameter (Parameters::MIX);
            if (param != nullptr)
                param->setValueNotifyingHost (param->convertTo0to1 (0.0f));

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;

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

            expect (maxSample < 0.001f,
                "Silent input with mix=0 should produce silence, max was "
                + juce::String (maxSample));
        }

        // Sample rate tests
        const std::array<double, 3> sampleRates = { 44100.0, 48000.0, 96000.0 };
        for (auto sr : sampleRates)
        {
            beginTest ("Processes without crash at SR=" + juce::String (sr));
            processAtSampleRate (sr, 512);
        }

        // Buffer size tests
        const std::array<int, 5> bufferSizes = { 64, 128, 256, 512, 1024 };
        for (auto bs : bufferSizes)
        {
            beginTest ("Processes without crash at bufferSize=" + juce::String (bs));
            processAtSampleRate (44100.0, bs);
        }

        beginTest ("Output is not all zeros with wet signal");
        {
            WetStringReverbProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            auto* param = processor.apvts.getParameter (Parameters::MIX);
            if (param != nullptr)
                param->setValueNotifyingHost (param->convertTo0to1 (1.0f));

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;

            buffer.clear();
            buffer.getWritePointer (0)[0] = 1.0f;
            buffer.getWritePointer (1)[0] = 1.0f;
            processor.processBlock (buffer, midi);

            float totalEnergy = 0.0f;
            for (int b = 0; b < 10; ++b)
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

            // Set all float params to max
            const char* floatParams[] = {
                Parameters::PRE_DELAY, Parameters::DECAY,
                Parameters::DAMPING, Parameters::BANDWIDTH,
                Parameters::SIZE, Parameters::MIX,
                Parameters::INPUT_DIFF_1, Parameters::INPUT_DIFF_2,
                Parameters::DECAY_DIFF_1, Parameters::DECAY_DIFF_2,
                Parameters::MOD_RATE, Parameters::MOD_DEPTH
            };

            for (auto* id : floatParams)
            {
                auto* p = processor.apvts.getParameter (id);
                if (p != nullptr)
                    p->setValueNotifyingHost (1.0f);
            }

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            buffer.clear();
            buffer.getWritePointer (0)[0] = 1.0f;
            buffer.getWritePointer (1)[0] = 1.0f;

            for (int b = 0; b < 20; ++b)
            {
                processor.processBlock (buffer, midi);
                buffer.clear();
            }

            expect (true, "Extreme parameters did not crash");
        }
    }

private:
    void processAtSampleRate (double sr, int blockSize)
    {
        WetStringReverbProcessor processor;
        processor.prepareToPlay (sr, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        buffer.clear();
        buffer.getWritePointer (0)[0] = 1.0f;
        buffer.getWritePointer (1)[0] = 1.0f;
        processor.processBlock (buffer, midi);

        for (int b = 0; b < 10; ++b)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
        }

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
