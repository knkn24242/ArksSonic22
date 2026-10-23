cmake_minimum_required(VERSION 3.22)

# macOS: must be set before project(). The AU runs on macOS 10.13+ (Intel) and 11+ (Apple Silicon).
# Building needs Xcode 12.4+ (JUCE 9), so older Macs use the GitHub Actions build.
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

project(ArksSonic22 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 9.0.3: external/JUCE (build_mac.sh clones it there), otherwise the checkout vendored by
# Simple Spectrum Analyzer next to this folder. Override with -DJUCE_DIR=...
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    set(JUCE_DIR_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE")
else()
    set(JUCE_DIR_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../SimpleSpectrumAnalyzer/external/JUCE")
endif()
set(JUCE_DIR "${JUCE_DIR_DEFAULT}" CACHE PATH "JUCE checkout")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

juce_add_plugin(ArksSonic22
    COMPANY_NAME "Arks Plugin"
    BUNDLE_ID com.arksplugin.arkssonic22
    PLUGIN_MANUFACTURER_CODE Arks
    PLUGIN_CODE Ss22
    FORMATS AU VST3 Standalone          # AU is only built on macOS; ignored elsewhere
    PRODUCT_NAME "Arks Sonic 22"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(ArksSonic22
    PRIVATE
        Source/AmpEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(ArksSonic22 PRIVATE Source)

target_compile_definitions(ArksSonic22
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ArksSonic22
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline tests (link the plugin's shared code).
option(AS22_BUILD_TESTS "Build offline tests" ON)
if(AS22_BUILD_TESTS)
    foreach(test_name PassthroughTest DspTest CpuBenchmark EditorSnapshot)
        add_executable(${test_name} Tests/${test_name}.cpp)
        target_include_directories(${test_name} PRIVATE Source
            $<TARGET_PROPERTY:ArksSonic22,INCLUDE_DIRECTORIES>)
        target_compile_definitions(${test_name} PRIVATE
            $<TARGET_PROPERTY:ArksSonic22,COMPILE_DEFINITIONS>)
        target_compile_features(${test_name} PRIVATE cxx_std_17)
        target_link_libraries(${test_name} PRIVATE ArksSonic22)
    endforeach()
endif()

# Hosts the built VST3 like a DAW (JUCE VST3 hosting). Compiles its own copy of the JUCE modules.
if(AS22_BUILD_TESTS AND (WIN32 OR APPLE))
    add_executable(Vst3HostTest Tests/Vst3HostTest.cpp)
    target_compile_definitions(Vst3HostTest PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LV2=0
        JUCE_PLUGINHOST_LADSPA=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1)
    target_compile_features(Vst3HostTest PRIVATE cxx_std_17)
    target_link_libraries(Vst3HostTest PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_recommended_config_flags)
endif()
