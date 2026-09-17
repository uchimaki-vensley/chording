cmake_minimum_required(VERSION 3.22)

project(Chording VERSION 0.2.0 LANGUAGES CXX)

option(CHORDING_BUILD_PLUGIN "Build the JUCE VST3 plugin" ON)
option(CHORDING_BUILD_TESTS "Build the chord detector tests" ON)
option(CHORDING_BUILD_MIDI_INSERT "Build the Windows Cubase MIDI Insert" ON)

if (CHORDING_BUILD_PLUGIN)
    include(FetchContent)

    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        # JUCE 8.0.13, pinned to an immutable commit for reproducible builds.
        GIT_TAG 7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2
    )
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(Chording
        COMPANY_NAME "Chording"
        BUNDLE_ID "dev.chording.plugin"
        PLUGIN_MANUFACTURER_CODE Chrd
        PLUGIN_CODE Chdg
        FORMATS VST3
        PRODUCT_NAME "Chording"
        DESCRIPTION "Live MIDI chord analyser and progression capture"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES Fx Analyzer
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
    )

    juce_generate_juce_header(Chording)

    target_sources(Chording
        PRIVATE
            Source/ChordInputState.cpp
            Source/ChordDetector.cpp
            Source/HarmonyAdvisor.cpp
            Source/PluginEditor.cpp
            Source/PluginProcessor.cpp
    )

    target_include_directories(Chording PRIVATE Source)
    target_compile_features(Chording PUBLIC cxx_std_20)
    if (MSVC)
        target_compile_options(Chording PRIVATE /utf-8)
    endif()

    target_compile_definitions(Chording
        PUBLIC
            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_REPORT_APP_USAGE=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )

    target_link_libraries(Chording
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

    if (CHORDING_BUILD_MIDI_INSERT AND WIN32 AND MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_library(ChordingMidiInsert SHARED
            Source/MidiInsert.cpp Source/MidiInsertState.cpp Source/MidiInsert.def)
        target_include_directories(ChordingMidiInsert PRIVATE
            Source
            ThirdParty/vst-ma
            ${JUCE_SOURCE_DIR}/modules
            $<TARGET_PROPERTY:Chording,JUCE_GENERATED_SOURCES_DIRECTORY>
        )
        target_compile_features(ChordingMidiInsert PRIVATE cxx_std_20)
        target_compile_options(ChordingMidiInsert PRIVATE /utf-8 /EHsc)
        target_compile_definitions(ChordingMidiInsert PRIVATE WIN32 _CRT_SECURE_NO_WARNINGS)
        target_link_libraries(ChordingMidiInsert PRIVATE Chording)
    endif()
endif()

if (CHORDING_BUILD_TESTS)
    enable_testing()
    if (TARGET ChordingMidiInsert)
        add_executable(MidiInsertTests Tests/MidiInsertTests.cpp)
        target_include_directories(MidiInsertTests PRIVATE ThirdParty/vst-ma)
        target_compile_features(MidiInsertTests PRIVATE cxx_std_20)
        target_compile_definitions(MidiInsertTests PRIVATE WIN32)
        target_compile_options(MidiInsertTests PRIVATE /W4 /utf-8 /EHsc)
        target_link_libraries(MidiInsertTests PRIVATE ChordingMidiInsert)
        add_test(NAME MidiInsertTests COMMAND MidiInsertTests)
    endif()
    add_executable(ChordDetectorTests
        Source/ChordInputState.cpp
        Source/MidiInsertState.cpp
        Source/ChordDetector.cpp
        Source/HarmonyAdvisor.cpp
        Tests/ChordDetectorTests.cpp
    )
    target_include_directories(ChordDetectorTests PRIVATE Source)
    target_compile_features(ChordDetectorTests PRIVATE cxx_std_20)
    if (MSVC)
        target_compile_options(ChordDetectorTests PRIVATE /utf-8 /EHsc)
    endif()
    add_test(NAME ChordDetectorTests COMMAND ChordDetectorTests)
endif()
