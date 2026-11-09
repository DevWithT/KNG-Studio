#pragma once

#include <tracktion_engine/tracktion_engine.h>

class KngSamplerPlugin;

class EditSession : private juce::Timer
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
    void addMidiTrack();
    bool isMidiTrack (int index);
    void deleteTrack (int index);
    void moveTrack (int index, int direction);
    void setFocusedTrack (int index)            { focusedTrack = index; }

    // Inputs (audio inputs for audio tracks, MIDI inputs for MIDI tracks)
    juce::StringArray getInputNames (int trackIndex);
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
    void clearSelection();
    void toggleClipSelection (tracktion::Clip* clip);
    void addClipsToSelection (const juce::Array<tracktion::Clip*>& clips);
    bool isClipSelected (tracktion::Clip* clip) const;
    std::vector<tracktion::Clip::Ptr> getSelectedClips() const;
    void selectAllClips();
    void deleteSelectedClip();
    void splitSelectedClipAtPlayhead();
    void duplicateSelectedClip();
    void copySelectedClip();
    void pasteClip();
    void beginEdit();

    // MIDI and instruments
    tracktion::MidiClip* createMidiClip (int trackIndex, double startSeconds);
    void setInstrument (int trackIndex, const juce::PluginDescription& desc);
    tracktion::Plugin* getInstrument (int trackIndex);
    void removeInstrument (int trackIndex);

    // Lets the UI open a piano roll (MainComponent sets this)
    std::function<void (tracktion::MidiClip&)> onShowPianoRoll;

    // Automation
    struct AutomationTarget
    {
        juce::String id, name;
        tracktion::AutomatableParameter* param = nullptr;
    };

    std::vector<AutomationTarget> getAutomationTargets (int trackIndex);
    tracktion::AutomatableParameter* getAutomationParam (int trackIndex);
    juce::String getAutomationParamId (int trackIndex);
    void setAutomationParamId (int trackIndex, const juce::String& id);
    bool isAutomationShown (int trackIndex);
    void setAutomationShown (int trackIndex, bool shouldShow);
    void setAutomationWrite (bool shouldWrite);
    bool isAutomationWrite();

    // Lets the UI open the comp window for a clip with takes (MainComponent sets this)
    std::function<void (tracktion::WaveAudioClip&)> onShowComp;

    // Punch in/out (uses the same region as the loop)
    void setPunchEnabled (bool shouldPunch);
    bool isPunchEnabled();

    // Track chain templates
    static juce::File getTemplatesFolder();
    juce::StringArray getTemplateNames();
    bool saveChainTemplate (int trackIndex, const juce::String& name);
    bool loadChainTemplate (int trackIndex, const juce::String& name);

    // KngTune
    bool addKngTune (int trackIndex);

    // Clip stretch, pitch and gain
    double getClipSourceBpm (tracktion::AudioClipBase& clip);
    void setClipFollowTempo (tracktion::AudioClipBase& clip, bool follow, double sourceBpm);
    void setClipPitch (tracktion::AudioClipBase& clip, float semitones);
    void setClipGain (tracktion::AudioClipBase& clip, float db);
    std::function<void (tracktion::AudioClipBase&)> onShowClipProps;

    // Markers
    std::vector<tracktion::Clip::Ptr> getMarkers();
    void addMarkerAt (double seconds);
    void jumpToMarker (int direction);

    // Autosave and crash recovery
    bool didRecoverFromCrash() const            { return recoveredFromCrash; }
    juce::File getBackupsFolder() const;
    void makeBackup();

    // Drums
    static juce::File getKitsFolder();
    static int getPadNote (int pad)             { return 36 + pad; }
    void addDrumTrack();
    bool isDrumTrack (int trackIndex);
    tracktion::SamplerPlugin* getDrumSampler (int trackIndex);
    juce::String getPadName (int trackIndex, int pad);
    void setPadName (int trackIndex, int pad, const juce::String& name);
    bool setPadSample (int trackIndex, int pad, const juce::File& file);
    void clearPad (int trackIndex, int pad);
    float getPadGain (int trackIndex, int pad);
    float getPadPan (int trackIndex, int pad);
    void setPadGainAndPan (int trackIndex, int pad, float gainDb, float pan);
    int getPadPitch (int trackIndex, int pad);
    void setPadPitch (int trackIndex, int pad, int semitones);
    int loadKitFolder (int trackIndex, const juce::File& folder);
    void playPad (int trackIndex, int pad, int velocity);
    tracktion::MidiClip* createPatternClip (int trackIndex, double startSeconds, int bars);
    std::function<void (int)> onShowDrumRack;

    // Built-in sampler instrument, chopping, and quick piano roll access
    void setBuiltInSampler (int trackIndex);
    KngSamplerPlugin* getKngSampler (int trackIndex);
    bool setSamplerSample (int trackIndex, const juce::File& file, int rootNote);
    juce::String getSamplerSampleName (int trackIndex);
    int getSamplerRoot (int trackIndex);
    void setSamplerRoot (int trackIndex, int rootNote);
    float getSamplerGain (int trackIndex);
    void setSamplerGain (int trackIndex, float gainDb);
    bool getSamplerOneShot (int trackIndex);
    void setSamplerOneShot (int trackIndex, bool oneShot);
    int chopClipToPads (tracktion::AudioClipBase& clip, int slices, bool atMarkers);
    void openPianoRollForTrack (int trackIndex);
    std::function<void (int)> onShowSampler;

    /** One sound in the Rack: a new track with a one-shot KNG Sampler. Returns the track index, or -1. */
    int addSampleChannel (const juce::File& sample);

    /** Volume from -inf (at or below -60) up to +12 dB. Above +6 uses a hidden boost stage. */
    float getTrackVolumeDb (int trackIndex);
    void setTrackVolumeDb (int trackIndex, float db);
    float getMasterVolumeDb();
    void setMasterVolumeDb (float db);

    // Patterns (the Rack). Copies of a pattern are linked: edit one, they all update.
    int getCurrentPattern() const               { return currentPattern; }
    void setCurrentPattern (int pattern)        { currentPattern = juce::jmax (1, pattern); }
    int getNumPatterns();
    int newPattern();
    int getPatternBars (int pattern);
    void setPatternBars (int pattern, int bars);
    double getPatternHome (int pattern);
    tracktion::MidiClip* getPatternClip (int trackIndex, int pattern, bool createIfMissing);
    void placePattern (int pattern, double atSeconds);
    void loopPattern (int pattern);
    void makePatternUnique (tracktion::MidiClip& clip);
    double getBarSeconds();

    // The Patterns strip on the timeline
    struct PatternBlock { int pattern = 0; double start = 0.0, end = 0.0; };
    std::vector<PatternBlock> getPatternBlocks();
    void removePatternAt (double seconds);

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
    tracktion::Plugin* getMasterMeterPlugin();

    // Lets the UI open an FX chain window (MainComponent sets this)
    std::function<void (tracktion::PluginList&, const juce::String&)> onShowFxChain;

    // Recording latency offset (saved between launches)
    static double getLatencyOffsetMs();
    void setLatencyOffsetMs (double ms);

    // Export
    static juce::File getLameExecutable();
    juce::File getExportsFolder();
    bool trackHasContent (int trackIndex);
    std::shared_ptr<tracktion::EditRenderer::Handle> exportMix (const juce::File& dest, bool loopOnly, int normaliseMode,
                                                                bool asMp3, std::function<void (const juce::String&)> onDone);
    std::shared_ptr<tracktion::EditRenderer::Handle> exportStem (int trackIndex, const juce::File& dest, bool loopOnly,
                                                                 std::function<void (const juce::String&)> onDone);

    // Undo
    void undo();
    void redo();

    // Import
    bool importAudioFile (const juce::File& file);
    double importAudioFileAt (const juce::File& file, int trackIndex, double startSeconds);

    void refreshInputs();
    void save();

private:
    void syncTrackState();
    juce::Array<tracktion::InputDeviceInstance*> getInputsFor (int trackIndex);
    void applyArm (int trackIndex, bool shouldBeArmed);
    tracktion::AuxSendPlugin* findSend (tracktion::AudioTrack& track);
    void ensureReverbBus();
    tracktion::Renderer::Parameters makeRenderParams (const juce::File& dest, bool loopOnly, int normaliseMode,
                                                      juce::AudioFormat& format);
    tracktion::TimeRange getExportRange (bool loopOnly);
    juce::File copyIntoProject (const juce::File& file);
    int getFirstFxIndex (tracktion::AudioTrack& track);
    void timerCallback() override;
    juce::File getLockFile() const;
    void pruneBackups();
    tracktion::MidiClip* restorePatternStash (int trackIndex, int pattern, double atSeconds);

    tracktion::Engine& engine;
    juce::File editFile;
    std::unique_ptr<tracktion::Edit> edit;

    std::vector<int> trackInputs;
    std::vector<bool> trackArmed;

    bool monitoringEnabled = true;
    tracktion::Clip::Ptr selectedClip;
    std::vector<tracktion::Clip::Ptr> copiedClips;
    std::vector<tracktion::Clip::Ptr> selection;
    int focusedTrack = -1;

    double viewStart = 0.0;
    double viewLength = 60.0;
    bool snapEnabled = true;
    bool recoveredFromCrash = false;
    juce::Time lastBackupTime;
    int currentPattern = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditSession)
};
