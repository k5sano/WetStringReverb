#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Parameters
{

// Parameter IDs — Dattorro / Clouds Plate Reverb
inline constexpr const char* PRE_DELAY        = "pre_delay";
inline constexpr const char* DECAY            = "decay";
inline constexpr const char* DAMPING          = "damping";
inline constexpr const char* BANDWIDTH        = "bandwidth";
inline constexpr const char* SIZE             = "size";
inline constexpr const char* MIX              = "mix";
inline constexpr const char* INPUT_DIFF_1     = "input_diffusion_1";
inline constexpr const char* INPUT_DIFF_2     = "input_diffusion_2";
inline constexpr const char* DECAY_DIFF_1     = "decay_diffusion_1";
inline constexpr const char* DECAY_DIFF_2     = "decay_diffusion_2";
inline constexpr const char* MOD_RATE         = "mod_rate";
inline constexpr const char* MOD_DEPTH        = "mod_depth";
inline constexpr const char* OVERSAMPLING     = "oversampling";

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { PRE_DELAY, 1 },
        "Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 200.0f, 0.1f),
        10.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { DECAY, 1 },
        "Decay",
        juce::NormalisableRange<float> (0.0f, 0.9999f, 0.001f),
        0.85f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { DAMPING, 1 },
        "Damping",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
        0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { BANDWIDTH, 1 },
        "Bandwidth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f),
        0.9995f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { SIZE, 1 },
        "Size",
        juce::NormalisableRange<float> (0.5f, 2.0f, 0.01f),
        1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { MIX, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
        0.35f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { INPUT_DIFF_1, 1 },
        "Input Diffusion 1",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        0.75f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { INPUT_DIFF_2, 1 },
        "Input Diffusion 2",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        0.625f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { DECAY_DIFF_1, 1 },
        "Decay Diffusion 1",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        0.7f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { DECAY_DIFF_2, 1 },
        "Decay Diffusion 2",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { MOD_RATE, 1 },
        "Mod Rate",
        juce::NormalisableRange<float> (0.01f, 5.0f, 0.01f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { MOD_DEPTH, 1 },
        "Mod Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
        0.5f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { OVERSAMPLING, 1 },
        "Oversampling",
        juce::StringArray { "Off", "2x", "4x" },
        0));

    return { params.begin(), params.end() };
}

}  // namespace Parameters
