cmake_minimum_required(VERSION 3.22)

project(VelvetUnderDron VERSION 1.0.0)

# JUCE を add_subdirectory で取り込む（JUCE サブモジュールを前提）
add_subdirectory(JUCE)

# C++17 必須
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# プラグイン定義
juce_add_plugin(VelvetUnderDron
    COMPANY_NAME "K5SANO"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE K5sn
    PLUGIN_CODE Vudr
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "VelvetUnderDron"
)

# ソースファイル
target_sources(VelvetUnderDron
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/DelayLine.cpp
        Source/DSP/FeedbackMatrix.cpp
        Source/DSP/AttenuationFilter.cpp
        Source/DSP/Saturation.cpp
        Source/DSP/SaturationToneFilter.cpp
        Source/DSP/Diffuser.cpp
        Source/DSP/VelvetNoise.cpp
        Source/DSP/EarlyReflections.cpp
        Source/DSP/FDNReverb.cpp
        Source/DSP/DarkVelvetNoise.cpp
        Source/DSP/OversamplingManager.cpp
        Source/DSP/ReverbMixer.cpp
)

# インクルードパス
target_include_directories(VelvetUnderDron
    PRIVATE
        Source
)

# プリプロセッサ定義
target_compile_definitions(VelvetUnderDron
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

if(MSVC)
    target_compile_options(VelvetUnderDron PRIVATE /utf-8)
endif()

# リンク
target_link_libraries(VelvetUnderDron
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# テストビルド
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()

    juce_add_console_app(VelvetUnderDronTests
        PRODUCT_NAME "VelvetUnderDronTests"
    )

    target_sources(VelvetUnderDronTests
        PRIVATE
            Tests/PluginTests.cpp
            Tests/FDNTests.cpp
            Tests/OversamplingTests.cpp
            Tests/SaturationTests.cpp
            Tests/AudioProcessingTests.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
            Source/DSP/DelayLine.cpp
            Source/DSP/FeedbackMatrix.cpp
            Source/DSP/AttenuationFilter.cpp
            Source/DSP/Saturation.cpp
            Source/DSP/SaturationToneFilter.cpp
            Source/DSP/Diffuser.cpp
            Source/DSP/VelvetNoise.cpp
            Source/DSP/EarlyReflections.cpp
            Source/DSP/FDNReverb.cpp
            Source/DSP/DarkVelvetNoise.cpp
            Source/DSP/OversamplingManager.cpp
            Source/DSP/ReverbMixer.cpp
    )

    target_include_directories(VelvetUnderDronTests
        PRIVATE
            Source
    )

    target_compile_definitions(VelvetUnderDronTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
    )

    if(MSVC)
        target_compile_options(VelvetUnderDronTests PRIVATE /utf-8)
    endif()

    target_link_libraries(VelvetUnderDronTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_test(NAME VelvetUnderDronTests COMMAND VelvetUnderDronTests)
endif()
