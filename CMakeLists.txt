cmake_minimum_required(VERSION 3.22)
project(noclip.DSP VERSION 0.4.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
option(NOCLIP_BUILD_PLUGIN "Build JUCE VST3 and standalone" ON)
set(NOCLIP_FONT_FILE "" CACHE FILEPATH "Optional locally supplied Counter-Strike TTF; not redistributed by this repository")
set(NOCLIP_JUCE_PATH "" CACHE PATH "Optional local JUCE 9.0.5 checkout")

add_library(noclip_dsp STATIC Source/DSP/AudioEngine.cpp Source/DSP/AdvancedDSP.cpp)
target_include_directories(noclip_dsp PUBLIC Source)
set_target_properties(noclip_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(noclip_dsp PRIVATE /W4 /permissive-)
else()
    target_compile_options(noclip_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(BUILD_TESTING)
    add_executable(DSPTests Tests/DSPTests.cpp)
    target_link_libraries(DSPTests PRIVATE noclip_dsp)
    add_test(NAME DSPTests COMMAND DSPTests)
    add_executable(ClipperTests Tests/ClipperTests.cpp)
    target_link_libraries(ClipperTests PRIVATE noclip_dsp)
    add_test(NAME ClipperTests COMMAND ClipperTests)
    add_executable(AdvancedTests Tests/AdvancedTests.cpp)
    target_link_libraries(AdvancedTests PRIVATE noclip_dsp)
    add_test(NAME AdvancedTests COMMAND AdvancedTests)
endif()
add_executable(DSPBenchmarks Benchmarks/DSPBenchmarks.cpp)
target_link_libraries(DSPBenchmarks PRIVATE noclip_dsp)

if(NOCLIP_BUILD_PLUGIN)
    if(NOCLIP_JUCE_PATH)
        add_subdirectory("${NOCLIP_JUCE_PATH}" "${CMAKE_BINARY_DIR}/juce")
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG d60b7a78030661574a615b586d9325ba7a7fa7dd)
        FetchContent_MakeAvailable(JUCE)
    endif()
    juce_add_plugin(noclip.DSP
        COMPANY_NAME "noclip.DSP"
        BUNDLE_ID "audio.noclipdsp.plugin"
        PLUGIN_MANUFACTURER_CODE Nclp
        PLUGIN_CODE Ncl1
        FORMATS VST3 Standalone
        PRODUCT_NAME "noclip.DSP"
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Distortion)
    target_sources(noclip.DSP PRIVATE
        Source/PluginProcessor.cpp Source/PluginEditor.cpp
        Source/Parameters/ParameterLayout.cpp Source/UI/TacticalLookAndFeel.cpp
        Source/DSP/ProcessingChain.cpp Source/UI/AdvancedPanel.cpp)
    juce_add_binary_data(noclip_artwork HEADER_NAME NoclipArtwork.h
        NAMESPACE NoclipArtwork SOURCES Assets/icon.png)
    target_link_libraries(noclip.DSP PRIVATE noclip_artwork)
    if(NOCLIP_FONT_FILE)
        if(NOT EXISTS "${NOCLIP_FONT_FILE}")
            message(FATAL_ERROR "NOCLIP_FONT_FILE does not exist")
        endif()
        juce_add_binary_data(noclip_font_data
            HEADER_NAME NoclipFontData.h NAMESPACE NoclipFontData
            SOURCES "${NOCLIP_FONT_FILE}")
        target_link_libraries(noclip.DSP PRIVATE noclip_font_data)
        target_compile_definitions(noclip.DSP PRIVATE NOCLIP_CUSTOM_FONT=1)
    else()
        target_compile_definitions(noclip.DSP PRIVATE NOCLIP_CUSTOM_FONT=0)
    endif()
    target_compile_definitions(noclip.DSP PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(noclip.DSP PRIVATE noclip_dsp
        juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    if(BUILD_TESTING)
        add_executable(StateTests Tests/StateTests.cpp Tests/ProcessingTests.cpp)
        target_include_directories(StateTests PRIVATE Source)
        target_link_libraries(StateTests PRIVATE noclip.DSP juce::juce_audio_utils)
        add_test(NAME StateTests COMMAND StateTests)
    endif()
endif()
