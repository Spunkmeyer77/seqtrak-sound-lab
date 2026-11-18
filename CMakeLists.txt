cmake_minimum_required(VERSION 3.22)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
endif()
project(SEQTRAKEditor VERSION 0.6.1 LANGUAGES C CXX)

set(SEQTRAK_JUCE_DIR "" CACHE PATH "Optional path to a JUCE 8.0.8 checkout")
if(SEQTRAK_JUCE_DIR)
    add_subdirectory("${SEQTRAK_JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(formats VST3 Standalone)
if(APPLE)
    list(APPEND formats AU)
endif()

juce_add_plugin(SEQTRAKEditor
    COMPANY_NAME "Transistor Labs"
    COMPANY_WEBSITE "https://ticcoross.gumroad.com/"
    COMPANY_EMAIL "ticcoross@gmail.com"
    BUNDLE_ID com.transistorlabs.seqtrakeditor
    PLUGIN_MANUFACTURER_CODE TrLb
    PLUGIN_CODE SqEd
    PRODUCT_NAME "SEQTRAK Editor"
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    FORMATS ${formats}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    VST3_CATEGORIES Instrument Tools
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(SEQTRAKAssets SOURCES
    Assets/TransistorLabs.svg Assets/Presets.json Assets/VoiceSchema.json Assets/Effects.json)
target_sources(SEQTRAKEditor PRIVATE Source/Plugin.cpp Source/Hardware.h Source/Sequencer.h Source/PatternJson.h)
target_compile_features(SEQTRAKEditor PRIVATE cxx_std_17)
target_compile_definitions(SEQTRAKEditor PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SEQTRAKEditor PRIVATE SEQTRAKAssets juce::juce_audio_utils
    juce::juce_recommended_config_flags)
