cmake_minimum_required(VERSION 3.22)
project(KNGStudio VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libs/tracktion_engine/modules/juce)

juce_add_module(libs/tracktion_engine/modules/tracktion_core ALIAS_NAMESPACE tracktion)
juce_add_module(libs/tracktion_engine/modules/tracktion_graph ALIAS_NAMESPACE tracktion)
juce_add_module(libs/tracktion_engine/modules/tracktion_engine ALIAS_NAMESPACE tracktion)

juce_add_gui_app(KNGStudio
    PRODUCT_NAME "KNG Studio"
    COMPANY_NAME "DevWithT")

target_sources(KNGStudio PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/EditSession.cpp
    Source/EditSessionTransport.cpp
    Source/EditSessionProject.cpp
    Source/EditSessionView.cpp
    Source/TransportBar.cpp
    Source/TrackLane.cpp
    Source/TrackLaneMouse.cpp
    Source/TrackHeader.cpp
    Source/TracksView.cpp
    Source/TimelineRuler.cpp)

target_compile_definitions(KNGStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_ASIO=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1)

target_link_libraries(KNGStudio PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_graph
    tracktion::tracktion_engine
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(KNGStudio PRIVATE /bigobj)
endif()
