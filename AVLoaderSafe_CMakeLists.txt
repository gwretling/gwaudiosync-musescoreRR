cmake_minimum_required(VERSION 3.24)

project(AudioVideoLoader VERSION 0.5.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(GW_USE_LOCAL_JUCE "Use a local JUCE checkout if present" ON)
set(GW_JUCE_VERSION "8.0.10" CACHE STRING "JUCE version to use")
set(GW_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")

if(GW_USE_LOCAL_JUCE)
  if(GW_JUCE_DIR AND EXISTS "${GW_JUCE_DIR}/CMakeLists.txt")
    message(STATUS "Using JUCE from GW_JUCE_DIR=${GW_JUCE_DIR}")
    add_subdirectory("${GW_JUCE_DIR}" JUCE)
  elseif(DEFINED ENV{JUCE_DIR} AND EXISTS "$ENV{JUCE_DIR}/CMakeLists.txt")
    message(STATUS "Using JUCE from JUCE_DIR=$ENV{JUCE_DIR}")
    add_subdirectory("$ENV{JUCE_DIR}" JUCE)
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
    message(STATUS "Using JUCE from bundled third_party/JUCE")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE" JUCE)
  else()
    include(FetchContent)
    message(STATUS "No local JUCE found. Fetching JUCE ${GW_JUCE_VERSION} from GitHub...")
    FetchContent_Declare(
      JUCE
      GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG ${GW_JUCE_VERSION}
      GIT_SHALLOW TRUE
      GIT_PROGRESS TRUE
      GIT_SUBMODULES_RECURSE TRUE
    )
    FetchContent_MakeAvailable(JUCE)
  endif()
else()
  include(FetchContent)
  FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${GW_JUCE_VERSION}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    GIT_SUBMODULES_RECURSE TRUE
  )
  FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(AudioVideoLoader
  COMPANY_NAME "GW"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Gwrt
  PLUGIN_CODE Avld
  FORMATS VST3
  PRODUCT_NAME "Audio & Video Loader"
  VST3_CATEGORIES Fx
)

juce_generate_juce_header(AudioVideoLoader)

target_sources(AudioVideoLoader
  PRIVATE
    src/PluginProcessor.h
    src/PluginProcessor.cpp
    src/PluginEditor.h
    src/PluginEditor.cpp
)

target_compile_definitions(AudioVideoLoader
  PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
)

target_link_libraries(AudioVideoLoader
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_video
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
