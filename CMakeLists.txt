cmake_minimum_required(VERSION 3.22)

project(WetStringReverb VERSION 1.0.0)

add_subdirectory(JUCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

juce_add_plugin(WetStringReverb
    COMPANY_NAME "K5SANO"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE K5sn
    PLUGIN_CODE Wsrv
    FORMATS VST3 Standalone
    PRODUCT_NAME "WetStringReverb"
)

target_sources(WetStringReverb
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/CloudsReverb.cpp
)

target_include_directories(WetStringReverb
    PRIVATE
        Source
)

target_compile_definitions(WetStringReverb
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

if(MSVC)
    target_compile_options(WetStringReverb PRIVATE /utf-8)
endif()

target_link_libraries(WetStringReverb
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()

    juce_add_console_app(WetStringReverbTests
        PRODUCT_NAME "WetStringReverbTests"
    )

    target_sources(WetStringReverbTests
        PRIVATE
            Tests/PluginTests.cpp
            Tests/AudioProcessingTests.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
            Source/DSP/CloudsReverb.cpp
    )

    target_include_directories(WetStringReverbTests
        PRIVATE
            Source
    )

    target_compile_definitions(WetStringReverbTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
    )

    if(MSVC)
        target_compile_options(WetStringReverbTests PRIVATE /utf-8)
    endif()

    target_link_libraries(WetStringReverbTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_test(NAME WetStringReverbTests COMMAND WetStringReverbTests)
endif()
