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
    Source/EditSessionTracks.cpp
    Source/TransportBar.cpp
    Source/TrackLane.cpp
    Source/TrackLaneMouse.cpp
    Source/TrackHeader.cpp
    Source/TracksView.cpp
    Source/TimelineRuler.cpp
    Source/EditSessionMixer.cpp
    Source/LevelMeter.cpp
    Source/FxChainComponent.cpp
    Source/MasterStrip.cpp
    Source/PluginWindow.cpp
    Source/EditSessionExport.cpp
    Source/ExportDialog.cpp
    Source/EditSessionLatency.cpp
    Source/LatencyTester.cpp
    Source/LatencyDialog.cpp
    Source/EditSessionMidi.cpp
    Source/PianoRoll.cpp
    Source/EditSessionAutomation.cpp
    Source/AutomationLane.cpp
    Source/MixerView.cpp
    Source/CompView.cpp
    Source/EditSessionExtras.cpp
    Source/EditSessionSelection.cpp
    Source/EditSessionClips.cpp
    Source/MarkerBar.cpp
    Source/ClipProps.cpp
    Source/EditSessionBackup.cpp
    Source/EditSessionDrums.cpp
    Source/KitBrowser.cpp
    Source/DrumRack.cpp
    Source/StepSequencer.cpp
    Source/PianoRollPaint.cpp
    Source/TheoryPanel.cpp
    Source/EditSessionSampler.cpp
    Source/SamplerView.cpp
    Source/DockPanel.cpp
    Source/EditSessionPatterns.cpp
    Source/RackRow.cpp
    Source/RackView.cpp
    Source/RackRowActions.cpp
    Source/EditSessionChannels.cpp
    Source/EditSessionVolume.cpp
    Source/EditSessionPatternLane.cpp
    Source/PatternLane.cpp
    Source/KngSamplerPlugin.cpp
    Source/KngSamplerView.cpp
    Source/StartScreen.cpp
    Source/EditSessionTools.cpp
    Source/GenericPluginPanel.cpp
    Source/EditSessionRouting.cpp
    Source/SettingsComponent.cpp
    Source/KngSynthPlugin.cpp
    Source/KngSynthView.cpp
    Source/EditSessionSynth.cpp
    Source/KngEqPlugin.cpp
    Source/KngEqView.cpp
    Source/EditSessionPlugins.cpp
    Source/PluginBrowser.cpp
    Source/KngCompPlugin.cpp
    Source/KngCompView.cpp
    Source/KngVerbPlugin.cpp
    Source/KngVerbView.cpp
    Source/KngDelayPlugin.cpp
    Source/KngDelayView.cpp
    Source/KngSatPlugin.cpp
    Source/KngSatView.cpp)

target_compile_definitions(KNGStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_ASIO=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_LAME_AUDIO_FORMAT=1
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
