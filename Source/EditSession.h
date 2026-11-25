#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include <vector>
#include "SliceFx.h"

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
    void selectTrack (int trackIndex)          { selectedTrackPtr = getTrack (trackIndex); routeLiveMidi(); }

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
    /** Per-lane Mon button: hear this lane's input live while it is armed. Arming turns it on, disarming turns it off. */
    void setTrackMonitor (int trackIndex, bool shouldMonitor);
    bool isTrackMonitor (int trackIndex);
    void updateMonitoring();

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
    /** Starts a new undo step. The name shows in Undo History. */
    void beginEdit (const juce::String& stepName = {});

    // MIDI and instruments
    tracktion::MidiClip* createMidiClip (int trackIndex, double startSeconds);
    void setInstrument (int trackIndex, const juce::PluginDescription& desc);
    tracktion::Plugin* getInstrument (int trackIndex);
    void removeInstrument (int trackIndex);
    /** The selected instrument track plays from any connected MIDI keyboard or pad, with no arming. */
    void routeLiveMidi();
    /** Plays or releases a note on the selected instrument track (the computer keyboard piano). */
    void playLiveNote (int note, bool down);
    /** Switches on and monitors any MIDI keyboard or pad plugged in while the Studio is open, then routes it. */
    void refreshMidiInputs();

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
    /** Crossfades every pair of overlapping audio clips on a lane. Returns how many it did. */
    int crossfadeOverlaps (int trackIndex);
    /** Lines a double or ad-lib up with the lead vocal on another lane: slides the whole clip, then
        cuts it at its gaps and nudges each phrase onto the lead. Returns how many moves it made, 0 if
        it was already tight, -1 if it couldn't (no audio to compare). */
    int alignClipToTrack (tracktion::WaveAudioClip& dub, int leadTrackIndex);
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
    /** Any other KNG instrument by type (for example "kng808"). Replaces the track's instrument. */
    void setBuiltInInstrument (int trackIndex, const juce::String& type);
    bool setSamplerSample (int trackIndex, const juce::File& file, int rootNote);
    juce::String getSamplerSampleName (int trackIndex);
    int getSamplerRoot (int trackIndex);
    void setSamplerRoot (int trackIndex, int rootNote);
    float getSamplerGain (int trackIndex);
    void setSamplerGain (int trackIndex, float gainDb);
    bool getSamplerOneShot (int trackIndex);
    void setSamplerOneShot (int trackIndex, bool oneShot);
    int chopClipToPads (tracktion::AudioClipBase& clip, int slices, bool atMarkers);

    /** Hit positions in a clip, in seconds from the clip start (the start itself is not included).
        Sensitivity runs 0 to 1. Returns at most 15 positions. */
    std::vector<double> findHits (tracktion::AudioClipBase& clip, float sensitivity);

    /** Cut times are timeline seconds. The first is the clip start, the last is the clip end.
        Makes one Rack row per slice (max 16) and mutes the clip. Returns the first new row, or -1. */
    int chopClipAtCuts (tracktion::AudioClipBase& clip, const std::vector<double>& cuts);

    /** Same cuts as above, but loads slice 1 on pad 1, slice 2 on pad 2 and so on, on a Drum Kit track.
        Returns how many pads were loaded, or -1. */
    int slicesToPads (tracktion::AudioClipBase& clip, const std::vector<double>& cuts, int trackIndex);
    /** Reverse, pitch and volume per slice (index 0 is slice 1) for the next chopClipAtCuts or slicesToPads. The Slice Editor sets it before the call and clears it after. */
    void setSliceFx (std::vector<SliceFx> fx)   { sliceFxForNextWrite = std::move (fx); }

    /** Writes one slice of a clip to its own 24 bit wav in the project Audio folder. Empty File on failure. */
    juce::File writeSliceFile (tracktion::AudioClipBase& clip, double fromTime, double toTime, int number);
    void openPianoRollForTrack (int trackIndex);

    // Note tools for one sound's notes in the current pattern (Rack row menu)
    /** Nudges timing (up to maxShiftBeats either way) and velocity (up to velocityRange either way). */
    void humanizePattern (int trackIndex, double maxShiftBeats, int velocityRange);
    /** Turns each chord into a run of single notes. direction: 0 up, 1 down, 2 up-down. */
    void arpeggiatePattern (int trackIndex, int direction, double stepBeats);
    /** Sets every note's length in beats. 0 = legato (each note lasts until the next one starts). */
    void setPatternNoteLength (int trackIndex, double beats);
    // The same tools on any clip (the Piano Roll uses these)
    void humanizeClip (tracktion::MidiClip& clip, double maxShiftBeats, int velocityRange);
    void arpeggiateClip (tracktion::MidiClip& clip, int direction, double stepBeats);
    void setClipNoteLength (tracktion::MidiClip& clip, double beats);
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
    /** Buses this lane sends to, in the order the sends sit in its chain. */
    juce::Array<int> getSendBuses (int trackIndex);
    bool isSendPreFader (int trackIndex, int bus);
    void setSendPreFader (int trackIndex, int bus, bool preFader);
    bool isSendMuted (int trackIndex, int bus);
    void setSendMuted (int trackIndex, int bus, bool shouldMute);
    void removeSend (int trackIndex, int bus);
    /** Buses this lane can output to or send to: not itself, and nothing that already feeds it. */
    juce::Array<int> getBusTargets (int trackIndex);
    /** Names of the lanes feeding a bus, by output or by send. */
    juce::StringArray getBusSources (int bus);
    /** True if `from` can safely output to `dest`: not itself, and `dest` doesn't already feed `from` by output or by a send. */
    bool canFeed (tracktion::AudioTrack& from, tracktion::AudioTrack& dest);

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
    /** Removes a pattern everywhere (its notes, every block, its stash). Later patterns move down one number. */
    bool deletePattern (int pattern);

    /** Each pattern keeps a copy in the "bank", far past the song, so new patterns never land on each other. */
    double getBankStart();
    /** Where the song ends, not counting the bank. */
    double getSongEnd();
    /** The end of the live part of the Playlist: the last bar with anything in it, or the first 16 bars of a new song. */
    double getPlaylistEnd();

    // The Playlist: lanes are the audio tracks, and pattern blocks sit in a lane
    bool isLaneTrack (int trackIndex);
    juce::String getLaneId (int trackIndex);
    int getLaneTrackIndex (const juce::String& laneId);
    juce::String getDefaultLaneId();
    void placePatternInLane (int pattern, double atSeconds, const juce::String& laneId);
    /** While painting blocks, hold off saving to disk until the stroke ends (saving on every block is slow). */
    void setDeferSave (bool shouldDefer)        { deferSave = shouldDefer; }
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

    /** Older songs: blocks of different patterns stacked on the same bars go into their own lane. */
    void separateOverlappingBlocks();

    // Tempo, click, count-in
    void setBpm (double bpm);
    double getBpm() const;
    void setClickEnabled (bool shouldBeOn);
    bool isClickEnabled() const;
    void setCountInBars (int bars);
    int getCountInBars() const;
    /** Record quantize: when recording stops, each recorded note's start snaps to this grid. 0 = off, otherwise the grid size in beats (1 = quarter note, 0.25 = 16th). Saved with the song. */
    double getRecordQuantize() const;
    void setRecordQuantize (double beats);

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
    /** Snap grid: divisions per beat (1 = beats, 2 = 8ths, 4 = 16ths, 8 = 32nds), 0 = whole bars. Saved with the song. */
    int getSnapGrid() const;
    void setSnapGrid (int perBeat);

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
    /** Master chain: 0 removes it, 1 Streaming Ready (about -14 LUFS), 2 Loud (about -9 LUFS). Clicking again replaces it. */
    void applyMasterChain (int style);

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
    void mergePatternTake (int pattern);
    void quantiseRecordedTake();

    tracktion::Engine& engine;
    juce::File editFile;
    std::unique_ptr<tracktion::Edit> edit;

    std::vector<int> trackInputs;
    std::vector<bool> trackArmed;
    juce::StringArray knownMidiInputs;   // MIDI inputs seen at the last scan, so only new ones get switched on

    bool monitoringEnabled = false;
    tracktion::Clip::Ptr selectedClip;
    std::vector<tracktion::Clip::Ptr> copiedClips;
    std::vector<tracktion::Clip::Ptr> selection;
    int focusedTrack = -1;

    double viewStart = 0.0;
    double viewLength = 60.0;
    bool snapEnabled = true;
    double playlistEndCache = 0.0;
    std::vector<SliceFx> sliceFxForNextWrite;
    juce::uint32 playlistEndStamp = 0;
    bool recoveredFromCrash = false;
    juce::Time lastBackupTime;
    int currentPattern = 1;
    bool patternMode = false;
    int playlistTool = 0;
    bool deferSave = false;
    double recordStartSeconds = 0.0;
    int recordingPattern = 0;   // set while recording notes into a pattern (Pattern mode)
    std::vector<tracktion::EditItemID> clipsBeforeRecording;
    std::shared_ptr<tracktion::EditRenderer::Handle> bounceHandle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditSession)
};
