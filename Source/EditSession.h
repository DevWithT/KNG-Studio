#pragma once

#include <tracktion_engine/tracktion_engine.h>

class EditSession
{
public:
    explicit EditSession (tracktion::Engine& engineToUse);
    ~EditSession();

    tracktion::Edit& getEdit()                  { return *edit; }
    tracktion::TransportControl& getTransport() { return edit->getTransport(); }

    // Transport
    void play();
    void stop();
    void record();

    // Tracks
    int getNumTracks();
    tracktion::AudioTrack* getTrack (int index);
    void addTrack();
    void deleteTrack (int index);

    // Inputs
    juce::StringArray getInputNames();
    int getTrackInput (int trackIndex) const;
    void setTrackInput (int trackIndex, int inputIndex);

    // Arming
    void setArmed (int trackIndex, bool shouldBeArmed);
    bool isArmed (int trackIndex) const;
    bool isAnyTrackArmed() const;

    // Monitoring
    void setMonitoring (bool shouldMonitor);
    bool isMonitoring() const                   { return monitoringEnabled; }

    // Clip selection
    void selectClip (tracktion::Clip* clip);
    tracktion::Clip* getSelectedClip() const    { return selectedClip.get(); }
    void deleteSelectedClip();

    // Tempo, click, count-in
    void setBpm (double bpm);
    double getBpm() const;
    void setClickEnabled (bool shouldBeOn);
    bool isClickEnabled() const;
    void setCountInBars (int bars);
    int getCountInBars() const;

    // Loop
    void setLooping (bool shouldLoop);
    bool isLooping();
    void setLoopRange (double startSeconds, double endSeconds);
    double getLoopStart();
    double getLoopEnd();

    // Undo
    void undo();
    void redo();

    // Import
    bool importAudioFile (const juce::File& file);

    void refreshInputs();
    void save();

private:
    void syncTrackState();
    juce::Array<tracktion::InputDeviceInstance*> getWaveInputs();
    void applyArm (int trackIndex, bool shouldBeArmed);

    tracktion::Engine& engine;
    std::unique_ptr<tracktion::Edit> edit;

    std::vector<int> trackInputs;
    std::vector<bool> trackArmed;

    bool monitoringEnabled = true;
    tracktion::Clip::Ptr selectedClip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditSession)
};
