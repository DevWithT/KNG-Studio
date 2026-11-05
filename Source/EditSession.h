#pragma once

#include <tracktion_engine/tracktion_engine.h>

class EditSession
{
public:
    explicit EditSession (tracktion::Engine& engineToUse);
    ~EditSession();

    tracktion::Edit& getEdit()                  { return *edit; }
    tracktion::TransportControl& getTransport() { return edit->getTransport(); }

    void play();
    void stop();
    void record();

    void setArmed (bool shouldBeArmed);
    bool isArmed() const                        { return armed; }

    void refreshInputs();
    void save();

private:
    tracktion::AudioTrack* getTrack();
    tracktion::InputDeviceInstance* findFirstWaveInput();

    tracktion::Engine& engine;
    std::unique_ptr<tracktion::Edit> edit;
    bool armed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditSession)
};
