cmake_minimum_required(VERSION 3.24)
# Must precede project(). Pass -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" for a universal build.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project(ScoreBackingNative VERSION 0.2.0 LANGUAGES C CXX)
# JUCE builds its juceaide helper in a separate CMake run that ignores the cache
# variable; without this it targets the SDK's macOS 15, where JUCE 7 can't compile.
if(APPLE)
    set(ENV{MACOSX_DEPLOYMENT_TARGET} "${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# JUCE 7.0.12 is pinned so this GPL-3.0 project has a reproducible toolchain.
set(JUCE_PATH "" CACHE PATH "Optional local JUCE 7.0.12 checkout")
if(JUCE_PATH)
    set(JUCE_SOURCE_CHECKOUT "${JUCE_PATH}")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7.0.12
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
    FetchContent_GetProperties(JUCE SOURCE_DIR JUCE_SOURCE_CHECKOUT)
endif()

include(cmake/JuceShutdownCompatibility.cmake)

# Pitch-preserving time-stretch for practice speeds (MIT licensed, header-only).
include(FetchContent)
FetchContent_Declare(signalsmith-stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG 1.4.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(signalsmith-stretch)

juce_add_plugin(ScoreBacking
    COMPANY_NAME "AlltAWD"
    BUNDLE_ID "io.github.alltawd.scorebackingtrack"
    PLUGIN_MANUFACTURER_CODE Atwd
    PLUGIN_CODE Bktr
    PRODUCT_NAME "Score Backing Track"
    DESCRIPTION "Play a recording in sync with the host timeline"
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Generator)

target_sources(ScoreBacking PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)
target_include_directories(ScoreBacking PUBLIC src)
target_compile_definitions(ScoreBacking PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)
target_link_libraries(ScoreBacking PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_gui_extra
    PUBLIC signalsmith-stretch juce::juce_recommended_config_flags)

if(UNIX AND NOT APPLE)
    # MuseScore 4.7 may discard its deferred instance cleanup at shutdown and
    # dlclose the module while JUCE's threads still run. Keep code mapped until
    # process exit; normal instance/editor cleanup is unaffected.
    target_link_options(ScoreBacking_VST3 PRIVATE "LINKER:-z,nodelete")
endif()

option(BACKING_BUILD_TESTS "Build processor integration tests" ON)
if(BACKING_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(ScoreBackingTests PRODUCT_NAME "Score Backing Track Tests")
    target_sources(ScoreBackingTests PRIVATE tests/ProcessorTests.cpp)
    target_link_libraries(ScoreBackingTests PRIVATE ScoreBacking
        juce::juce_audio_processors juce::juce_audio_formats juce::juce_gui_extra)
    add_test(NAME processor COMMAND ScoreBackingTests)
endif()
