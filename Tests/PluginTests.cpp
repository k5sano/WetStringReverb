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
        WetStringReverbProcessor processor;
        auto& apvts = processor.apvts;

        beginTest ("All 13 parameters exist in APVTS");
        {
            const char* paramIds[] = {
                Parameters::PRE_DELAY, Parameters::DECAY,
                Parameters::DAMPING, Parameters::BANDWIDTH,
                Parameters::SIZE, Parameters::MIX,
                Parameters::INPUT_DIFF_1, Parameters::INPUT_DIFF_2,
                Parameters::DECAY_DIFF_1, Parameters::DECAY_DIFF_2,
                Parameters::MOD_RATE, Parameters::MOD_DEPTH,
                Parameters::OVERSAMPLING
            };

            int count = 0;
            for (auto* id : paramIds)
            {
                auto* param = apvts.getParameter (id);
                expect (param != nullptr, juce::String ("Parameter missing: ") + id);
                if (param != nullptr)
                    ++count;
            }
            expect (count == 13, "Expected 13 parameters, got " + juce::String (count));
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

            checkDefault (Parameters::PRE_DELAY, 10.0f);
            checkDefault (Parameters::DECAY, 0.85f);
            checkDefault (Parameters::DAMPING, 0.3f);
            checkDefault (Parameters::BANDWIDTH, 0.9995f, 0.001f);
            checkDefault (Parameters::SIZE, 1.0f);
            checkDefault (Parameters::MIX, 0.35f);
            checkDefault (Parameters::INPUT_DIFF_1, 0.75f);
            checkDefault (Parameters::INPUT_DIFF_2, 0.625f);
            checkDefault (Parameters::DECAY_DIFF_1, 0.7f);
            checkDefault (Parameters::DECAY_DIFF_2, 0.5f);
            checkDefault (Parameters::MOD_RATE, 1.0f);
            checkDefault (Parameters::MOD_DEPTH, 0.5f);
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
            WetStringReverbProcessor processor;
            expect (processor.getName() == "WetStringReverb",
                    "Plugin name should be WetStringReverb");
        }

        beginTest ("Plugin is stereo in/out");
        {
            WetStringReverbProcessor processor;
            expect (processor.getTotalNumInputChannels() >= 2,
                    "Should accept stereo input");
            expect (processor.getTotalNumOutputChannels() >= 2,
                    "Should produce stereo output");
        }

        beginTest ("Plugin does not accept MIDI");
        {
            WetStringReverbProcessor processor;
            expect (!processor.acceptsMidi(), "Should not accept MIDI");
            expect (!processor.producesMidi(), "Should not produce MIDI");
        }

        beginTest ("State save/restore works");
        {
            WetStringReverbProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            auto* param = processor.apvts.getParameter (Parameters::DECAY);
            if (param != nullptr)
                param->setValueNotifyingHost (param->convertTo0to1 (0.95f));

            juce::MemoryBlock state;
            processor.getStateInformation (state);

            WetStringReverbProcessor processor2;
            processor2.setStateInformation (state.getData(),
                                             static_cast<int> (state.getSize()));

            auto* param2 = processor2.apvts.getParameter (Parameters::DECAY);
            if (param2 != nullptr)
            {
                float restored = param2->convertFrom0to1 (param2->getValue());
                expectWithinAbsoluteError (restored, 0.95f, 0.01f,
                    "Decay should be restored to 0.95");
            }
        }
    }
};

static PluginBasicTests pluginBasicTests;

//==============================================================================
class ConsoleTestRunner : public juce::UnitTestRunner
{
    void logMessage (const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};

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
