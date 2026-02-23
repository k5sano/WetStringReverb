#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/Parameters.h"
#include <iostream>

//==============================================================================
class ParameterTests : public juce::UnitTest
{
public:
    ParameterTests() : juce::UnitTest ("Parameter Tests") {}

    void runTest() override
    {
        VelvetUnderDronProcessor processor;
        auto& apvts = processor.apvts;

        beginTest ("All 19 parameters exist in APVTS");
        {
            const char* paramIds[] = {
                Parameters::DRY_WET, Parameters::PRE_DELAY_MS,
                Parameters::EARLY_LEVEL_DB, Parameters::LATE_LEVEL_DB,
                Parameters::ROOM_SIZE, Parameters::STEREO_WIDTH,
                Parameters::OVERSAMPLING,
                Parameters::LOW_RT60_S, Parameters::HIGH_RT60_S,
                Parameters::HF_DAMPING, Parameters::DIFFUSION,
                Parameters::DECAY_SHAPE,
                Parameters::SAT_AMOUNT, Parameters::SAT_DRIVE_DB,
                Parameters::SAT_TYPE, Parameters::SAT_TONE,
                Parameters::SAT_ASYMMETRY,
                Parameters::MOD_DEPTH, Parameters::MOD_RATE_HZ
            };

            int count = 0;
            for (auto* id : paramIds)
            {
                auto* param = apvts.getParameter (id);
                expect (param != nullptr, juce::String ("Parameter missing: ") + id);
                if (param != nullptr)
                    ++count;
            }
            expect (count == 19, "Expected 19 parameters, got " + juce::String (count));
        }

        beginTest ("Default values are correct");
        {
            auto checkDefault = [&] (const char* id, float expected, float tolerance = 0.01f) {
                auto* param = apvts.getParameter (id);
                if (param != nullptr)
                {
                    float denormalized = param->convertFrom0to1 (param->getDefaultValue());
                    expectWithinAbsoluteError (denormalized, expected, tolerance,
                        juce::String (id) + " default should be " + juce::String (expected));
                }
            };

            checkDefault (Parameters::DRY_WET, 30.0f);
            checkDefault (Parameters::PRE_DELAY_MS, 12.0f);
            checkDefault (Parameters::EARLY_LEVEL_DB, -3.0f);
            checkDefault (Parameters::LATE_LEVEL_DB, -6.0f);
            checkDefault (Parameters::ROOM_SIZE, 0.6f);
            checkDefault (Parameters::STEREO_WIDTH, 70.0f);
            checkDefault (Parameters::LOW_RT60_S, 1.8f);
            checkDefault (Parameters::HIGH_RT60_S, 0.9f);
            checkDefault (Parameters::HF_DAMPING, 65.0f);
            checkDefault (Parameters::DIFFUSION, 80.0f);
            checkDefault (Parameters::DECAY_SHAPE, 40.0f);
            checkDefault (Parameters::SAT_AMOUNT, 0.0f);
            checkDefault (Parameters::SAT_DRIVE_DB, 6.0f);
            checkDefault (Parameters::SAT_TONE, 0.0f);
            checkDefault (Parameters::SAT_ASYMMETRY, 0.0f);
            checkDefault (Parameters::MOD_DEPTH, 15.0f);
            checkDefault (Parameters::MOD_RATE_HZ, 0.5f);
        }

        beginTest ("Parameter ranges are correct");
        {
            auto checkRange = [&] (const char* id, float minVal, float maxVal) {
                auto* param = apvts.getParameter (id);
                if (param != nullptr)
                {
                    auto range = param->getNormalisableRange();
                    expectWithinAbsoluteError (range.start, minVal, 0.01f,
                        juce::String (id) + " min should be " + juce::String (minVal));
                    expectWithinAbsoluteError (range.end, maxVal, 0.01f,
                        juce::String (id) + " max should be " + juce::String (maxVal));
                }
            };

            checkRange (Parameters::DRY_WET, 0.0f, 100.0f);
            checkRange (Parameters::PRE_DELAY_MS, 0.0f, 100.0f);
            checkRange (Parameters::EARLY_LEVEL_DB, -24.0f, 6.0f);
            checkRange (Parameters::LATE_LEVEL_DB, -24.0f, 6.0f);
            checkRange (Parameters::ROOM_SIZE, 0.1f, 1.0f);
            checkRange (Parameters::STEREO_WIDTH, 0.0f, 100.0f);
            checkRange (Parameters::LOW_RT60_S, 0.2f, 5.0f);
            checkRange (Parameters::HIGH_RT60_S, 0.1f, 3.0f);
            checkRange (Parameters::HF_DAMPING, 0.0f, 100.0f);
            checkRange (Parameters::SAT_AMOUNT, 0.0f, 100.0f);
            checkRange (Parameters::SAT_DRIVE_DB, 0.0f, 24.0f);
            checkRange (Parameters::SAT_TONE, -100.0f, 100.0f);
            checkRange (Parameters::SAT_ASYMMETRY, 0.0f, 100.0f);
            checkRange (Parameters::MOD_DEPTH, 0.0f, 100.0f);
            checkRange (Parameters::MOD_RATE_HZ, 0.1f, 5.0f);
        }

        beginTest ("Choice parameters have correct options");
        {
            // Oversampling: Off, 2x, 4x
            auto* osParam = dynamic_cast<juce::AudioParameterChoice*> (
                apvts.getParameter (Parameters::OVERSAMPLING));
            expect (osParam != nullptr, "Oversampling should be AudioParameterChoice");
            if (osParam != nullptr)
            {
                expect (osParam->choices.size() == 3, "Oversampling should have 3 choices");
                expect (osParam->getIndex() == 1, "Oversampling default should be 2x (index 1)");
            }

            // Saturation Type: Soft, Warm, Tape, Tube
            auto* satParam = dynamic_cast<juce::AudioParameterChoice*> (
                apvts.getParameter (Parameters::SAT_TYPE));
            expect (satParam != nullptr, "SatType should be AudioParameterChoice");
            if (satParam != nullptr)
            {
                expect (satParam->choices.size() == 4, "SatType should have 4 choices");
                expect (satParam->getIndex() == 1, "SatType default should be Warm (index 1)");
            }
        }
    }
};

static ParameterTests parameterTests;

//==============================================================================
class PluginBasicTests : public juce::UnitTest
{
public:
    PluginBasicTests() : juce::UnitTest ("Plugin Basic Tests") {}

    void runTest() override
    {
        beginTest ("Plugin name is correct");
        {
            VelvetUnderDronProcessor processor;
            expect (processor.getName() == "VelvetUnderDron",
                    "Plugin name should be VelvetUnderDron");
        }

        beginTest ("Plugin is stereo in/out");
        {
            VelvetUnderDronProcessor processor;
            expect (processor.getTotalNumInputChannels() >= 2,
                    "Should accept stereo input");
            expect (processor.getTotalNumOutputChannels() >= 2,
                    "Should produce stereo output");
        }

        beginTest ("Plugin does not accept MIDI");
        {
            VelvetUnderDronProcessor processor;
            expect (!processor.acceptsMidi(), "Should not accept MIDI");
            expect (!processor.producesMidi(), "Should not produce MIDI");
        }

        beginTest ("State save/restore works");
        {
            VelvetUnderDronProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            // パラメータを変更
            auto* param = processor.apvts.getParameter (Parameters::DRY_WET);
            if (param != nullptr)
                param->setValueNotifyingHost (param->convertTo0to1 (50.0f));

            // 状態を保存
            juce::MemoryBlock state;
            processor.getStateInformation (state);

            // 別のインスタンスに復元
            VelvetUnderDronProcessor processor2;
            processor2.setStateInformation (state.getData(),
                                             static_cast<int> (state.getSize()));

            auto* param2 = processor2.apvts.getParameter (Parameters::DRY_WET);
            if (param2 != nullptr)
            {
                float restored = param2->convertFrom0to1 (param2->getValue());
                expectWithinAbsoluteError (restored, 50.0f, 0.5f,
                    "DryWet should be restored to 50%");
            }
        }
    }
};

static PluginBasicTests pluginBasicTests;

//==============================================================================
// stdout にログ出力するカスタムランナー
class ConsoleTestRunner : public juce::UnitTestRunner
{
    void logMessage (const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};

// テストランナー（main）
int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    ConsoleTestRunner runner;
    runner.runAllTests();

    int numFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* result = runner.getResult (i);
        if (result != nullptr)
            numFailures += result->failures;
    }

    std::cout << "\n=== Total failures: " << numFailures << " ===" << std::endl;
    return numFailures > 0 ? 1 : 0;
}
