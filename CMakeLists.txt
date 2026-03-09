cmake_minimum_required(VERSION 3.22)

project(erhcetua VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to your local JUCE checkout, e.g. -DJUCE_DIR=/path/to/JUCE")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

function(configure_juce_plugin target_name)
    target_compile_definitions(${target_name}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(${target_name}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

juce_add_plugin(erhcetua
    COMPANY_NAME "md"
    BUNDLE_ID "com.md.erhcetua"
    IS_SYNTH FALSE
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE MdFm
    PLUGIN_CODE Erhc
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "erhcetua")

target_sources(erhcetua
    PRIVATE
        Source/ErhcetuaPluginEditor.cpp
        Source/ErhcetuaPluginEditor.h
        Source/ErhcetuaPluginProcessor.cpp
        Source/ErhcetuaPluginProcessor.h
        Source/ErhcetuaRhythmEngine.h)

configure_juce_plugin(erhcetua)

juce_add_plugin(GP6
    COMPANY_NAME "md"
    BUNDLE_ID "com.md.gp6"
    IS_SYNTH FALSE
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE MdFm
    PLUGIN_CODE Gp06
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "GP-6")

target_sources(GP6
    PRIVATE
        Source/Gp6PluginEditor.cpp
        Source/Gp6PluginEditor.h
        Source/Gp6PluginProcessor.cpp
        Source/Gp6PluginProcessor.h
        Source/Gp6PatchEngine.h)

configure_juce_plugin(GP6)
