#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Parameters
{

// Parameter ID constants
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

// Debug bypass switches
inline constexpr const char* BYPASS_EARLY       = "bypass_early";
inline constexpr const char* BYPASS_FDN         = "bypass_fdn";
inline constexpr const char* BYPASS_DVN         = "bypass_dvn";
inline constexpr const char* BYPASS_SATURATION  = "bypass_saturation";
inline constexpr const char* BYPASS_TONE_FILTER = "bypass_tone_filter";
inline constexpr const char* BYPASS_ATTEN_FILTER = "bypass_atten_filter";
inline constexpr const char* BYPASS_MODULATION  = "bypass_modulation";

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ---- Main controls (7) ----
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
        1));

    // ---- Reverb character (5) ----
    // *** RT60 ranges extended for long violin sustain ***
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { LOW_RT60_S, 1 },
        "Low RT60",
        juce::NormalisableRange<float> (0.2f, 12.0f, 0.01f, 0.4f),  // skew 0.4 = low end detail
        2.5f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { HIGH_RT60_S, 1 },
        "High RT60",
        juce::NormalisableRange<float> (0.1f, 8.0f, 0.01f, 0.4f),
        1.4f,
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

    // ---- Saturation (5) ----
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
        1));

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

    // ---- Modulation (2) ----
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

    // ---- Debug bypass switches (7) ----
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { BYPASS_EARLY, 1 },
        "Bypass Early Reflections", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { BYPASS_FDN, 1 },
        "Bypass FDN Reverb", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { BYPASS_DVN, 1 },
        "Bypass DVN Tail", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { BYPASS_SATURATION, 1 },
        "Bypass Saturation", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { BYPASS_TONE_FILTER, 1 },
        "Bypass Tone Filter", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { BYPASS_ATTEN_FILTER, 1 },
        "Bypass Attenuation Filter", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { BYPASS_MODULATION, 1 },
        "Bypass Modulation", false));

    return { params.begin(), params.end() };
}

}  // namespace Parameters
