#pragma once

#include <tracktion_engine/tracktion_engine.h>

class EditSession
{
public:
    EditSession (tracktion::Engine& engineToUse, const juce::File& editFileToUse);
    ~EditSession();

    tracktion::Edit& getEdit()                  { return *edit; }
    tracktion::TransportControl& getTransport() { return edit->getTransport(); }

    // Project
    const juce::File& getProjectFile() const    { return editFile; }
    juce::File getProjectFolder() const         { return editFile.getParentDirectory(); }
    juce::String getProjectName() const         { return editFile.getFileNameWithoutExtension(); }

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

    // Clip selection and editing
    void selectClip (tracktion::Clip* clip);
    tracktion::Clip* getSelectedClip() const    { return selectedClip.get(); }
    void deleteSelectedClip();
    void splitSelectedClipAtPlayhead();
    void beginEdit();

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

    // View (shared by the ruler and every lane)
    double getViewStart() const                 { return viewStart; }
    double getViewLength() const                { return viewLength; }
    void setView (double startSeconds, double lengthSeconds);
    void zoomAround (double timeSeconds, double factor);
    void scrollBy (double seconds);
    double xToTime (int x, int width) const;
    int timeToX (double seconds, int width) const;

    // Snap
    bool isSnapEnabled() const                  { return snapEnabled; }
    void setSnapEnabled (bool shouldSnap)       { snapEnabled = shouldSnap; }
    double snapTime (double seconds) const;

    // Mixer and plugins
    juce::Array<tracktion::Plugin*> getFxPlugins (tracktion::PluginList& list);
    void addPlugin (tracktion::PluginList& list, const juce::PluginDescription& desc);
    void movePlugin (tracktion::PluginList& list, tracktion::Plugin* plugin, int direction);
    void removePlugin (tracktion::Plugin* plugin);
    float getSendDb (int trackIndex);
    void setSendDb (int trackIndex, float db);
    bool isReverbBus (int trackIndex);
    void setMasterVolumeDb (float db);
    float getMasterVolumeDb();
    tracktion::Plugin* getMasterMeterPlugin();

    // Lets the UI open an FX chain window (MainComponent sets this)
    std::function<void (tracktion::PluginList&, const juce::String&)> onShowFxChain;

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
    tracktion::AuxSendPlugin* findSend (tracktion::AudioTrack& track);
    void ensureReverbBus();

    tracktion::Engine& engine;
    juce::File editFile;
    std::unique_ptr<tracktion::Edit> edit;

    std::vector<int> trackInputs;
    std::vector<bool> trackArmed;

    bool monitoringEnabled = true;
    tracktion::Clip::Ptr selectedClip;

    double viewStart = 0.0;
    double viewLength = 60.0;
    bool snapEnabled = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditSession)
};
