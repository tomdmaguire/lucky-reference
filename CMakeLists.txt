cmake_minimum_required(VERSION 3.25)

project(LuckyReference VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

set(LUCKY_LABS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lucky-labs" CACHE PATH "Lucky Labs shared code")
include("${LUCKY_LABS_DIR}/cmake/LuckyLabs.cmake")

option(LUCKY_REFERENCE_INSTALL_AFTER_BUILD "Copy the plug-ins into ~/Library/Audio/Plug-Ins after building" ON)

juce_add_plugin(LuckyReference
    COMPANY_NAME "Lucky Labs"
    BUNDLE_ID com.luckylabs.luckyreference
    PLUGIN_MANUFACTURER_CODE Lcky
    PLUGIN_CODE Lref
    FORMATS VST3 AU
    PRODUCT_NAME "Lucky Reference"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Analyzer
    COPY_PLUGIN_AFTER_BUILD ${LUCKY_REFERENCE_INSTALL_AFTER_BUILD}
)

set(LUCKY_REFERENCE_CORE_SOURCES
    Source/dsp/LoudnessMeter.cpp
    Source/ref/ReferenceTrack.cpp
    Source/ref/Resampler.cpp
    Source/ref/FileFormat.cpp
    Source/ref/ReferenceLoader.cpp
    Source/dsp/ReferencePlayer.cpp
    Source/dsp/ReferenceEngine.cpp
    Source/library/ReferenceLibrary.cpp
    Source/library/TonalProfile.cpp
)

target_sources(LuckyReference PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/SlotRow.cpp
    Source/ui/LibraryPanel.cpp
    Source/ui/SourceSwitch.cpp
    Source/ui/RadarRipples.cpp
    ${LUCKY_REFERENCE_CORE_SOURCES}
)

target_include_directories(LuckyReference PRIVATE Source)

target_compile_definitions(LuckyReference PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LuckyReference
    PRIVATE
        lucky_ui
        lucky_dsp
        LuckyLabsFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_console_app(LuckyReferenceTests PRODUCT_NAME "Lucky Reference Tests")

target_sources(LuckyReferenceTests PRIVATE
    Tests/TestMain.cpp
    Tests/LoudnessMeterTests.cpp
    Tests/ReferenceTrackTests.cpp
    Tests/ReferenceEngineTests.cpp
    Tests/LibraryTests.cpp
    Tests/TonalProfileTests.cpp
    ${LUCKY_REFERENCE_CORE_SOURCES}
)

target_include_directories(LuckyReferenceTests PRIVATE Source)

target_compile_definitions(LuckyReferenceTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_UNIT_TESTS=1
)

target_link_libraries(LuckyReferenceTests
    PRIVATE
        lucky_dsp
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_data_structures
        "-framework AudioToolbox"
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

enable_testing()
add_test(NAME LuckyReferenceTests COMMAND $<TARGET_FILE:LuckyReferenceTests>)

add_executable(LuckyReferenceProcessorTests Tests/ProcessorTests.cpp)
target_include_directories(LuckyReferenceProcessorTests PRIVATE Source Tests $<TARGET_PROPERTY:LuckyReference,INCLUDE_DIRECTORIES>)
target_compile_definitions(LuckyReferenceProcessorTests PRIVATE JUCE_UNIT_TESTS=1 $<TARGET_PROPERTY:LuckyReference,COMPILE_DEFINITIONS>)
target_link_libraries(LuckyReferenceProcessorTests PRIVATE LuckyReference)
add_test(NAME LuckyReferenceProcessorTests COMMAND $<TARGET_FILE:LuckyReferenceProcessorTests>)

add_executable(LuckyReferenceSnapshot tools/Snapshot.cpp)
target_include_directories(LuckyReferenceSnapshot PRIVATE Source $<TARGET_PROPERTY:LuckyReference,INCLUDE_DIRECTORIES>)
target_compile_definitions(LuckyReferenceSnapshot PRIVATE $<TARGET_PROPERTY:LuckyReference,COMPILE_DEFINITIONS>)
target_link_libraries(LuckyReferenceSnapshot PRIVATE LuckyReference)

juce_add_console_app(LuckyReferenceLoadBenchmark PRODUCT_NAME "Lucky Reference Load Benchmark")
target_sources(LuckyReferenceLoadBenchmark PRIVATE tools/LoadBenchmark.cpp ${LUCKY_REFERENCE_CORE_SOURCES})
target_include_directories(LuckyReferenceLoadBenchmark PRIVATE Source)
target_compile_definitions(LuckyReferenceLoadBenchmark PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(LuckyReferenceLoadBenchmark
    PRIVATE lucky_dsp juce::juce_audio_formats juce::juce_dsp juce::juce_data_structures "-framework AudioToolbox"
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

juce_add_console_app(LuckyReferenceBench PRODUCT_NAME "Lucky Reference Bench")
target_sources(LuckyReferenceBench PRIVATE tools/Benchmark.cpp ${LUCKY_REFERENCE_CORE_SOURCES})
target_include_directories(LuckyReferenceBench PRIVATE Source)
target_compile_definitions(LuckyReferenceBench PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(LuckyReferenceBench
    PRIVATE lucky_dsp juce::juce_audio_formats juce::juce_dsp juce::juce_data_structures "-framework AudioToolbox"
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

add_executable(LuckyReferenceProfile tools/ProfileTool.cpp)
target_include_directories(LuckyReferenceProfile PRIVATE Source $<TARGET_PROPERTY:LuckyReference,INCLUDE_DIRECTORIES>)
target_compile_definitions(LuckyReferenceProfile PRIVATE $<TARGET_PROPERTY:LuckyReference,COMPILE_DEFINITIONS>)
target_link_libraries(LuckyReferenceProfile PRIVATE LuckyReference)
