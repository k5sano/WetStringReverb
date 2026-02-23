#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Parameters
{

// パラメータ ID 定数
inline constexpr const char* DRY_WET            = "dry_wet";
inline constexpr const char* PRE_DELAY_MS       = "pre_delay_ms";
inline constexpr const char* EARLY_LEVEL_DB     = "early_level_db";
inline constexpr const char* LATE_LEVEL_DB      = "late_level_db";
inline constexpr const char* ROOM_SIZE          = "room_size";
inline constexpr const char* STEREO_WIDTH       = "stereo_width";
inline constexpr const char* OVERSAMPLING       = "oversampling_factor";

inline constexpr const char* LOW_RT60_S         = "low_rt60_s";
inline constexpr const char* HIGH_RT60_S        = "high_rt60_s";
inline constexpr const char* HF_DAMPING         = "hf_damping";
inline constexpr const char* DIFFUSION          = "diffusion";
inline constexpr const char* DECAY_SHAPE        = "decay_shape";

inline constexpr const char* SAT_AMOUNT         = "sat_amount";
inline constexpr const char* SAT_DRIVE_DB       = "sat_drive_db";
inline constexpr const char* SAT_TYPE           = "sat_type";
inline constexpr const char* SAT_TONE           = "sat_tone";
inline constexpr const char* SAT_ASYMMETRY      = "sat_asymmetry";

inline constexpr const char* MOD_DEPTH          = "mod_depth";
inline constexpr const char* MOD_RATE_HZ        = "mod_rate_hz";

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ---- メインコントロール (7) ----
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { DRY_WET, 1 },
        "Dry/Wet Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        30.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { PRE_DELAY_MS, 1 },
        "Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        12.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { EARLY_LEVEL_DB, 1 },
        "Early Level",
        juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f),
        -3.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { LATE_LEVEL_DB, 1 },
        "Late Level",
        juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f),
        -6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ROOM_SIZE, 1 },
        "Room Size",
        juce::NormalisableRange<float> (0.1f, 1.0f, 0.01f),
        0.6f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { STEREO_WIDTH, 1 },
        "Stereo Width",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        70.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { OVERSAMPLING, 1 },
        "Oversampling",
        juce::StringArray { "Off", "2x", "4x" },
        1));  // default: 2x

    // ---- 残響特性 (5) ----
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { LOW_RT60_S, 1 },
        "Low RT60",
        juce::NormalisableRange<float> (0.2f, 5.0f, 0.01f),
        1.8f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { HIGH_RT60_S, 1 },
        "High RT60",
        juce::NormalisableRange<float> (0.1f, 3.0f, 0.01f),
        0.9f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { HF_DAMPING, 1 },
        "HF Damping",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        65.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { DIFFUSION, 1 },
        "Diffusion",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        80.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { DECAY_SHAPE, 1 },
        "Decay Shape",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        40.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // ---- サチュレーション (5) ----
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { SAT_AMOUNT, 1 },
        "Saturation Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { SAT_DRIVE_DB, 1 },
        "Saturation Drive",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f),
        6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { SAT_TYPE, 1 },
        "Saturation Type",
        juce::StringArray { "Soft", "Warm", "Tape", "Tube" },
        1));  // default: Warm

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { SAT_TONE, 1 },
        "Saturation Tone",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { SAT_ASYMMETRY, 1 },
        "Saturation Asymmetry",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // ---- モジュレーション (2) ----
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { MOD_DEPTH, 1 },
        "Mod Depth",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        15.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { MOD_RATE_HZ, 1 },
        "Mod Rate",
        juce::NormalisableRange<float> (0.1f, 5.0f, 0.01f),
        0.5f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    return { params.begin(), params.end() };
}

}  // namespace Parameters
