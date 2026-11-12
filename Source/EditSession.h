#pragma once

#include <tracktion_engine/tracktion_engine.h>

class KngSamplerPlugin;
class KngLoudnessPlugin;

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
    /** While recording: throw away this take, go back to where it started, and record again. */
    void retake();

    /** MIDI learn: while on, move a knob in KNG Studio, then a knob on your controller, to link them. */
    void setMidiLearn (bool shouldLearn);
    bool isMidiLearn();

    /** The track the inspector on the right is showing (-1 if none). */
    void selectTrack (int trackIndex)          { selectedTrackPtr = getTrack (trackIndex); }

    int getSelectedTrackIndex()
    {
        for (int i = 0; i < getNumTracks(); ++i)
            if (selectedTrackPtr != nullptr && getTrack (i) == selectedTrackPtr.get())
                return i;

        return -1;
    }

    tracktion::Track::Ptr selectedTrackPtr;

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
    void setBuiltInSynth (int trackIndex);
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

    // Plugin picker. An id is "kngsynth", "kngsampler", a built-in effect type, or a VST's identifier string.
    int addInstrumentTrack (const juce::String& pluginId, const juce::String& trackName);
    void setInstrumentById (int trackIndex, const juce::String& pluginId);
    void addEffectById (int trackIndex, const juce::String& pluginId);

    /** Volume from -inf (at or below -60) up to +12 dB. Above +6 uses a hidden boost stage. */
    float getTrackVolumeDb (int trackIndex);
    void setTrackVolumeDb (int trackIndex, float db);
    float getMasterVolumeDb();
    void setMasterVolumeDb (float db);

    // Built-in effects: list of { menu name, plugin type }
    static std::vector<std::pair<juce::String, juce::String>> getBuiltInEffects();
    void addBuiltInPlugin (tracktion::PluginList& list, const juce::String& type);

    // Render a track (with its effects) to audio on a new track, and mute the original
    void bounceTrackInPlace (int trackIndex);

    // Move one copy of a pattern (every track's clip in it) to a new spot
    void movePatternBlock (int pattern, double fromSeconds, double toSeconds);

    // Buses and sends (bus 0 is the original reverb bus)
    int addBus (const juce::String& name);
    juce::Array<int> getBusNumbers();
    juce::String getBusName (int bus);
    float getSendToBusDb (int trackIndex, int bus);
    void setSendToBusDb (int trackIndex, int bus, float db);

    // Sidechain ducking: a compressor on the target track, triggered by the source (kick ducks the 808)
    void duckFromTrack (int targetIndex, int sourceIndex);
    void removeDucking (int targetIndex);
    int getDuckSource (int targetIndex);   // -1 if none

    // Track folders: tracks with the same folder name sit together and collapse as one
    juce::String getTrackFolder (int trackIndex);
    void setTrackFolder (int trackIndex, const juce::String& folder);
    juce::StringArray getFolderNames();
    juce::Array<int> getFolderTracks (const juce::String& folder);
    bool isFolderCollapsed (const juce::String& folder);
    void setFolderCollapsed (const juce::String& folder, bool collapsed);
    void renameFolder (const juce::String& from, const juce::String& to);
    void removeFolder (const juce::String& folder);
    void toggleFolderMute (const juce::String& folder);
    void toggleFolderSolo (const juce::String& folder);

    // Patterns (the Rack). Copies of a pattern are linked: edit one, they all update.
    int getCurrentPattern() const               { return currentPattern; }
    void setCurrentPattern (int pattern)        { currentPattern = juce::jmax (1, pattern); }
    int getNumPatterns();
    int newPattern();
    int getPatternBars (int pattern);
    /** 16th-note steps in a pattern, counted from its real bars (12 per bar in 3/4, 14 in 7/8). */
    int getPatternSteps (int pattern);
    void setPatternBars (int pattern, int bars);
    double getPatternHome (int pattern);
    tracktion::MidiClip* getPatternClip (int trackIndex, int pattern, bool createIfMissing);
    void placePattern (int pattern, double atSeconds);
    void loopPattern (int pattern);
    void makePatternUnique (tracktion::MidiClip& clip);
    double getBarSeconds();

    // The Patterns strip on the timeline
    struct PatternBlock { int pattern = 0; double start = 0.0, end = 0.0; juce::String lane; };
    std::vector<PatternBlock> getPatternBlocks();
    void removePatternAt (double seconds);
    float getPatternSwing (int pattern);
    void setPatternSwing (int pattern, float percent);

    // Pattern names and colours
    juce::String getPatternName (int pattern);
    void setPatternName (int pattern, const juce::String& name);
    juce::Colour getPatternColour (int pattern);
    void setPatternColour (int pattern, juce::Colour colour);
    int duplicatePattern (int pattern);

    /** Each pattern keeps a copy in the "bank", far past the song, so new patterns never land on each other. */
    double getBankStart();
    /** Where the song ends, not counting the bank. */
    double getSongEnd();

    // The Playlist: lanes are the audio tracks, and pattern blocks sit in a lane
    bool isLaneTrack (int trackIndex);
    juce::String getLaneId (int trackIndex);
    int getLaneTrackIndex (const juce::String& laneId);
    juce::String getDefaultLaneId();
    void placePatternInLane (int pattern, double atSeconds, const juce::String& laneId);
    void moveBlock (const PatternBlock& block, double toSeconds, const juce::String& toLane, bool copy);
    void deleteBlock (const PatternBlock& block);
    juce::Array<tracktion::Clip*> getBlockClips (const PatternBlock& block);

    // Pattern mode loops the pattern you're editing; Song mode plays the Playlist
    void setPatternMode (bool shouldLoopPattern);
    bool isPatternMode() const                  { return patternMode; }
    void cuePattern (int pattern, bool startPlaying);
    int getPlaylistTool() const                 { return playlistTool; }   // 0 = Select, 1 = Paint
    void setPlaylistTool (int tool)             { playlistTool = tool; }
    std::function<void()> onShowRack;

    // Playlist editing
    void cutBlock (const PatternBlock& block, double atSeconds);
    void resizeBlock (const PatternBlock& block, double newEnd);

    /** Keeps the Playlist tidy: loose MIDI clips become pattern blocks, and muted lanes silence their blocks. */
    void tidyPlaylist();

    // Tempo, click, count-in
    void setBpm (double bpm);
    double getBpm() const;
    void setClickEnabled (bool shouldBeOn);
    bool isClickEnabled() const;
    void setCountInBars (int bars);
    int getCountInBars() const;

    // Tempo map: tempo and time signature changes, with bars counted the musical way
    struct TempoMark
    {
        double seconds = 0.0;
        juce::String label;
        bool isTimeSig = false;
        int index = 0;
    };

    std::vector<TempoMark> getTempoMarks();
    void addTempoChange (double seconds, double bpm);
    void addTimeSigChange (double seconds, int numerator, int denominator);
    void removeTempoMark (const TempoMark& mark);
    double getBpmAt (double seconds);
    void setBpmAt (double seconds, double bpm);
    juce::String getTimeSigAt (double seconds);
    double barStartAt (double seconds);              // where the bar this time falls in begins
    double barsAfter (double barStart, int bars);    // the time that many bars later
    int barNumberAt (double seconds);                // counting from 1
    double snapToBeat (double seconds);

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
    /** The loudness meter at the end of the master chain (added the first time it's asked for). */
    KngLoudnessPlugin* getLoudnessMeter();

    // Lets the UI open an FX chain window (MainComponent sets this)
    std::function<void (tracktion::PluginList&, const juce::String&)> onShowFxChain;

    // Lets the UI open a KNG effect's own panel in the dock (MainComponent sets this)
    std::function<void (tracktion::Plugin&)> onShowPluginEditor;

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

    /** Render several tracks together into one file (a folder stem). */
    std::shared_ptr<tracktion::EditRenderer::Handle> exportTracks (const juce::Array<int>& trackIndices, const juce::File& dest,
                                                                   bool loopOnly, std::function<void (const juce::String&)> onDone);

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
    bool patternMode = false;
    int playlistTool = 0;
    double recordStartSeconds = 0.0;
    std::vector<tracktion::EditItemID> clipsBeforeRecording;
    std::shared_ptr<tracktion::EditRenderer::Handle> bounceHandle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditSession)
};
