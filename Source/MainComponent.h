#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <tracktion_engine/tracktion_engine.h>
#include "EditSession.h"
#include "TransportBar.h"
#include "TracksView.h"
#include "TimelineRuler.h"

class MainComponent : public juce::Component,
                      private juce::ChangeListener
{
public:
    explicit MainComponent (tracktion::Engine& engineToUse);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showAudioSettings();
    void updateDeviceLabel();

    tracktion::Engine& engine;
    juce::TextButton settingsButton { "Audio Settings" };
    juce::Label deviceLabel;

    std::unique_ptr<EditSession> session;
    std::unique_ptr<TransportBar> transportBar;
    std::unique_ptr<TimelineRuler> ruler;

    juce::Viewport tracksViewport;
    std::unique_ptr<TracksView> tracksView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
