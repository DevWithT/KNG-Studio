# KNG Studio map

## 1. Project
KNG Studio, a DAW. C++20, JUCE 8, Tracktion Engine, Windows, CMake.
Built with menu.bat (option 2 builds and runs, option 7 commits and pushes).
Fonts are compiled in through juce_add_binary_data (KNGStudioAssets).
All source files are in Source/.

## 2. Working rules
- Change only what a prompt asks for. Do not refactor or tidy anything else.
- Do not run bash, builds, type checks or servers unless a prompt asks.
- When you create or delete a source file, add or remove its line in this map in the same change.

## 3. File map

### App shell and windows
- `Main.cpp`: app entry point and main window.
- `MainComponent.cpp/.h`: main window layout; registers the built-in plugin types with the engine.
- `MainComponentProject.cpp`: project folders; open, close, new, save, restore backup, new-song setup.
- `MainComponentMenus.cpp`: menu bar names, contents and command handling.
- `MainComponentKeys.cpp`: keyboard shortcuts, shortcuts help, play/stop and zoom to fit.
- `MainComponentWindows.cpp`: dock panel openers and dialog windows.
- `StartScreen.cpp/.h`: opening screen: start something new or reopen a song.
- `SettingsComponent.cpp/.h`: settings for audio, plugins, recording, folders and about.
- `TransportBar.cpp/.h`: top bar with transport, tempo and song controls.
- `ExportDialog.cpp/.h`: export dialog for the mix and stems.
- `LatencyDialog.cpp/.h`: dialog for measuring and setting recording latency.
- `LatencyTester.cpp/.h`: plays a click and times how long it takes to come back in.
- `MidiMapDialog.cpp/.h`: lists every MIDI controller link in the song, with Remove.
- `PluginWindow.cpp/.h`: plugin editor windows, and KngUIBehaviour that hooks them into the engine.
- `PluginBrowser.cpp/.h`: every instrument and effect in one searchable list.
- `KitBrowser.cpp/.h`: left-side Sounds panel (kits, search, favorites) and SamplePreviewer.

### EditSession (the song model)
- `EditSession.cpp/.h`: the class itself; construction, tracks, inputs, arming, monitoring, clip select/delete.
- `EditSessionAutomation.cpp`: automation targets and per-track shown/write state.
- `EditSessionBackup.cpp`: lock file, timed backups and pruning old ones.
- `EditSessionChannels.cpp`: adds a sample channel to the Rack.
- `EditSessionClips.cpp`: audio clip tempo-follow, pitch and gain; song markers.
- `EditSessionDrums.cpp`: drum tracks, pads, kit folders, creating pattern clips.
- `EditSessionExport.cpp`: mix, stem and multi-track export; render settings.
- `EditSessionExtras.cpp`: punch recording, effect chain templates, KNG Tune.
- `EditSessionFolders.cpp`: track folders: membership, collapse, rename, mute, solo.
- `EditSessionInstruments.cpp`: puts a built-in instrument on a track.
- `EditSessionLatency.cpp`: recording latency offset.
- `EditSessionMidi.cpp`: MIDI tracks, MIDI clips and VST instruments.
- `EditSessionMixer.cpp`: effect list add/move/remove, reverb send and bus, master meters.
- `EditSessionPatternDelete.cpp`: deletes a pattern and renumbers the ones after it.
- `EditSessionPatternInfo.cpp`: pattern name, colour, home, bank start, song end, duplicate.
- `EditSessionPatternLane.cpp`: lists pattern blocks, removes a block, restores stashed clips.
- `EditSessionPatterns.cpp`: pattern count, bars, steps, swing, clips, place and loop.
- `EditSessionPlaylist.cpp`: lanes; placing, moving and deleting blocks; cue and pattern mode.
- `EditSessionPlaylistEdit.cpp`: cut and resize blocks; tidy the Playlist; separate overlaps.
- `EditSessionPlugins.cpp`: adds instruments and effects by id (built-in or VST).
- `EditSessionProject.cpp`: save, import audio, copy files into the project.
- `EditSessionRouting.cpp`: buses, sends to a bus, sidechain ducking.
- `EditSessionSampler.cpp`: KNG Sampler settings, chop a clip to pads, open the piano roll.
- `EditSessionSelection.cpp`: multi-clip selection.
- `EditSessionSynth.cpp`: puts KNG Synth on a track.
- `EditSessionTempo.cpp`: bar maths, tempo and time-signature changes, MIDI learn.
- `EditSessionTools.cpp`: built-in effects list, adding one, bounce in place, move pattern block.
- `EditSessionTracks.cpp`: move a track; duplicate, copy and paste a clip.
- `EditSessionTransport.cpp`: play, record, stop, BPM, click, count-in, loop, undo/redo, retake.
- `EditSessionView.cpp`: view range, zoom, scroll, x/time mapping, snap grid, beginEdit, split.
- `EditSessionVolume.cpp`: track and master volume in dB.

### Playlist
- `TracksView.cpp/.h`: the Playlist: one row per lane; handles plugin and pattern drops.
- `TrackHeader.cpp/.h`: a lane's header: colour strip, name, R M S, meter, input.
- `TrackLane.cpp/.h`: one lane's clip area.
- `TrackLaneMouse.cpp`: TrackLane hit testing and the click, drag, release path.
- `TrackLaneMouseActions.cpp`: TrackLane scroll wheel, keys, clip menu, audio file drops, double-click.
- `BlockLayer.cpp/.h`: sits over one lane and draws its pattern blocks.
- `AutomationLane.cpp/.h`: automation row: AutomationHeader (pick target) and the curve.
- `FolderHeader.cpp/.h`: bar above a folder's tracks: collapse, name, mute, solo.
- `TimelineRuler.cpp/.h`: the bar.beat ruler above the lanes.
- `MarkerBar.cpp/.h`: thin row above the ruler for song section markers.
- `PlaylistToolbar.h`: Select, Paint and Cut tools; keeps the Playlist tidy.
- `ClipProps.cpp/.h`: stretch, pitch and gain for one audio clip.
- `CompView.cpp/.h`: take comping for an audio clip, and its window.

### Rack and patterns
- `RackView.cpp/.h`: every sound and instrument in one place, editing the current pattern.
- `RackRow.cpp/.h`: one sound in the Rack: controls on the left, its pattern right.
- `RackRowPaint.cpp`: RackRow painting (drum rows, mini piano roll) and header layout.
- `RackRowMouse.cpp`: RackRow mouse: row clicks, step painting, dragging the pattern out.
- `RackRowActions.cpp`: RackRow's actions and menus.
- `PatternListPanel.cpp/.h`: column listing every pattern; click to edit, arrow loops it.
- `PatternGrid.h`: step-grid helpers shared by the Rack and drum sequencer (16th notes).
- `DrumRack.cpp/.h`: drum pads panel for the dock.

### Piano roll
- `PianoRoll.cpp/.h`: the piano roll note editor and its window.
- `PianoRollNotes.cpp`: note editing: hit testing, velocity, quantise, delete, erase, slide.
- `PianoRollInput.cpp`: piano roll mouse and keyboard handlers.
- `PianoRollPaint.cpp`: piano roll drawing.
- `TheoryPanel.cpp/.h`: right-hand panel that explains what you're looking at.
- `TheoryHelper.h`: small music theory toolkit (keys, scales, chords).

### Mixer, dock and inspector
- `MixerView.cpp/.h`: MixerStrip (one channel), MixerView (all strips) and MixerWindow.
- `MixerViewInserts.cpp`: MixerStrip insert slots, send buses, opening plugins, add-effect menu.
- `MasterStrip.cpp/.h`: the master channel strip.
- `LevelMeter.cpp/.h`: level meter component.
- `LoudnessDisplay.cpp/.h`: master loudness readout: momentary, short-term, integrated LUFS, peak.
- `KngLoudnessPlugin.cpp/.h`: invisible BS.1770 loudness meter at the end of the master chain.
- `SendPopup.h`: small pop-up with one send level slider.
- `FxChainComponent.cpp/.h`: a track's effect chain list with its add menu, and its window.
- `DockPanel.cpp/.h`: tabbed panel docked along the bottom, plus its resize divider.
- `TrackInspector.cpp/.h`: right-side panel: selected track's volume, instrument, effects, sends.
- `InspectorCards.cpp/.h`: KngIdentity (colour and chip per plugin, kngIdentityFor) and InstrumentCard.
- `InspectorParts.cpp/.h`: ParamKnob, EffectRow and a plugin's most useful knobs.
- `VolumeHelpers.h`: shared volume fader range: -inf to +12 dB, double-click for 0.

### Built-in instruments
- `Kng808Plugin.cpp/.h`: synthesized 808: pitch drop, length, punch, drive, glide.
- `Kng808View.cpp/.h`: KNG 808's face.
- `KngSynthPlugin.cpp/.h`: two-oscillator synth with sub, noise, filter, envelopes, LFO.
- `KngSynthPluginAudio.cpp`: KNG Synth voice rendering and the audio/MIDI process callback.
- `KngSynthView.cpp/.h`: KNG Synth's face.
- `KngSynthViewPaint.cpp`: KNG Synth face painting: cards, wave, filter, envelope, LFO.
- `KngSynthDsp.h`: synth engine: oscillators, envelopes, filter, voice.
- `KngSynthParams.h`: every KNG Synth knob: id, name, range, default, display.
- `KngSynthPresets.h`: KNG Synth factory sounds.
- `KngSamplerPlugin.cpp/.h`: one sample across the keyboard, with loop, glide, mono cut.
- `KngSamplerView.cpp/.h`: KNG Sampler's face: waveform, loop edges, keyboard.
- `KngSamplerViewPaint.cpp`: KNG Sampler face layout and painting: header, waveform, loop, playhead.
- `KngSamplerViewActions.cpp`: KNG Sampler face sample loading, peaks, timer, mouse, drag-and-drop.

### Built-in effects (plugin + view pairs)
- `KngFx.cpp/.h`: KngFxBase, KngParamDef, KngPresetDef: shared base for the smaller KNG effects.
- `KngEqPlugin.cpp/.h`: 8-band EQ with live analyzer.
- `KngEqView.cpp/.h`: KNG EQ's face: band dots dragged over a live spectrum.
- `KngEqViewPaint.cpp`: KNG EQ title and screen painting: grid, spectrum, curves, dots.
- `KngEqViewMouse.cpp`: KNG EQ screen mouse handling and the band right-click menu.
- `KngCompPlugin.cpp/.h`: compressor: four modes, soft knee, parallel mix, sidechain filter.
- `KngCompView.cpp/.h`: KNG Comp's face: VU needle beside the compression curve.
- `KngVerbPlugin.cpp/.h`: reverb: 8-line feedback delay network with ducking.
- `KngVerbView.cpp/.h`: KNG Verb's face: rings that grow with Size.
- `KngDelayPlugin.cpp/.h`: tempo-synced stereo, ping-pong or mono delay.
- `KngDelayView.cpp/.h`: KNG Delay's face: echoes as fading bars on the beat grid.
- `KngSatPlugin.cpp/.h`: saturator: four flavours, 4x oversampled.
- `KngSatView.cpp/.h`: KNG Saturator's face: glowing tube beside the drive curve.
- `KngDeEssPlugin.cpp/.h`: de-esser (KngFxBase).
- `KngDeEssView.cpp/.h`: KNG De-Esser's face: lit S band with a reduction history.
- `KngLimiterPlugin.cpp/.h`: look-ahead limiter (KngFxBase).
- `KngLimiterView.cpp/.h`: KNG Limiter's face: ceiling line over a reduction history.
- `KngDoublerPlugin.cpp/.h`: turns one vocal into several stereo takes (KngFxBase).
- `KngDoublerView.cpp/.h`: KNG Doubler's face: doubles drifting around the vocal.
- `KngFattenerPlugin.cpp/.h`: bigger 808s and kicks: sub, octave-down, punch, drive (KngFxBase).
- `KngFattenerView.cpp/.h`: KNG Fattener's face: one big FAT knob.
- `KngAirPlugin.cpp/.h`: air shelf, presence lift and Shine for vocals (KngFxBase).
- `KngAirView.cpp/.h`: KNG Air's face: lift curve with sparkles.
- `KngGatePlugin.cpp/.h`: gate / soft expander for gaps between phrases (KngFxBase).
- `KngGateView.cpp/.h`: KNG Gate's face: level history under the threshold line.
- `KngWidthPlugin.cpp/.h`: stereo width, mono bass and Haas spread (KngFxBase).
- `KngWidthView.cpp/.h`: KNG Width's face: stereo scope and mono-safety meter.
- `KngRadioPlugin.cpp/.h`: phone, radio, megaphone and walkie sounds (KngFxBase).
- `KngRadioView.cpp/.h`: KNG Radio's face: tuning dial and speaker grille.
- `KngThrowPlugin.cpp/.h`: tempo-locked echo that ducks while you sing (KngFxBase).
- `KngThrowView.cpp/.h`: KNG Throw's face: trail of echoes that dims while ducking.

### Shared UI (faces, look and feel, fonts, pop-outs)
- `KngFaceView.cpp/.h`: shared frame every KNG effect face is built on; kngMakeStyle.
- `KngFace.cpp/.h`: KngFaceStyle, KngFaceLook, KngFaceKnob, KngFaceToggle, KngFaceChoice, KngPopButton.
- `KngFxView.cpp/.h`: ready-made panel for KngFxBase effects, built from their parameters.
- `GenericPluginPanel.cpp/.h`: panel of sliders for plugins with no UI of their own.
- `KngLookAndFeel.h`: the app's one dark theme: buttons, knobs, sliders.
- `KngFonts.cpp/.h`: the fonts built into the app for the plugin faces.
- `KngPopOut.cpp/.h`: KNG plugin panels in their own windows; dock them back.
- `KngPresets.cpp/.h`: user presets saved in Documents\KNG Studio\Presets, and KngPresetMenu.

### DSP helpers
- `KngFilters.h`: KngDsp: Biquad filters and envelope Follower shared by newer plugins.
- `KngEqDsp.h`: KNG EQ parameter layout and filter maths.
- `KngCompDsp.h`: KNG Comp parameters, gain curve and presets.
- `KngVerbDsp.h`: KNG Verb parameters and presets.
- `KngDelayDsp.h`: KNG Delay parameters, note lengths and presets.
- `KngSatDsp.h`: KNG Saturator parameters, the four shapes and presets.

## 4. Where EditSession functions live
- `EditSession.cpp`: EditSession, ~EditSession, syncTrackState, getNumTracks, getTrack, addTrack, deleteTrack, getInputsFor, getInputNames, getTrackInput, setTrackInput, isArmed, isAnyTrackArmed, setArmed, applyArm, refreshInputs, setMonitoring, selectClip, deleteSelectedClip
- `EditSessionAutomation.cpp`: getAutomationTargets, getAutomationParam, getAutomationParamId, setAutomationParamId, isAutomationShown, setAutomationShown, setAutomationWrite, isAutomationWrite
- `EditSessionBackup.cpp`: getLockFile, getBackupsFolder, timerCallback, makeBackup, pruneBackups
- `EditSessionChannels.cpp`: addSampleChannel
- `EditSessionClips.cpp`: getClipSourceBpm, setClipFollowTempo, setClipPitch, setClipGain, getMarkers, addMarkerAt, jumpToMarker
- `EditSessionDrums.cpp`: getKitsFolder, isDrumTrack, getDrumSampler, getPadName, setPadName, setPadSample, clearPad, getPadGain, getPadPan, setPadGainAndPan, getPadPitch, setPadPitch, playPad, loadKitFolder, createPatternClip
- `EditSessionExport.cpp`: getLameExecutable, getExportsFolder, trackHasContent, getExportRange, makeRenderParams, exportMix, exportStem, exportTracks
- `EditSessionExtras.cpp`: setPunchEnabled, isPunchEnabled, getFirstFxIndex, getTemplatesFolder, getTemplateNames, saveChainTemplate, loadChainTemplate, addKngTune
- `EditSessionFolders.cpp`: getTrackFolder, getFolderTracks, getFolderNames, setTrackFolder, isFolderCollapsed, setFolderCollapsed, renameFolder, removeFolder, toggleFolderMute, toggleFolderSolo
- `EditSessionInstruments.cpp`: setBuiltInInstrument
- `EditSessionLatency.cpp`: getLatencyOffsetMs, setLatencyOffsetMs
- `EditSessionMidi.cpp`: addMidiTrack, isMidiTrack, createMidiClip, setInstrument, getInstrument, removeInstrument
- `EditSessionMixer.cpp`: getFxPlugins, addPlugin, movePlugin, removePlugin, findSend, isReverbBus, getSendDb, setSendDb, ensureReverbBus, getMasterMeterPlugin, getLoudnessMeter
- `EditSessionPatternDelete.cpp`: deletePattern
- `EditSessionPatternInfo.cpp`: getBankStart, getPatternHome, getSongEnd, getPatternName, setPatternName, getPatternColour, setPatternColour, duplicatePattern
- `EditSessionPatternLane.cpp`: getPatternBlocks, removePatternAt, restorePatternStash
- `EditSessionPatterns.cpp`: getBarSeconds, getNumPatterns, newPattern, getPatternBars, getPatternSteps, setPatternBars, getPatternClip, placePattern, loopPattern, makePatternUnique, getPatternSwing, setPatternSwing
- `EditSessionPlaylist.cpp`: isLaneTrack, getLaneId, getLaneTrackIndex, getDefaultLaneId, getBlockClips, placePatternInLane, moveBlock, deleteBlock, cuePattern, setPatternMode
- `EditSessionPlaylistEdit.cpp`: cutBlock, resizeBlock, tidyPlaylist, separateOverlappingBlocks
- `EditSessionPlugins.cpp`: setInstrumentById, addInstrumentTrack, addEffectById
- `EditSessionProject.cpp`: save, importAudioFile, copyIntoProject, importAudioFileAt
- `EditSessionRouting.cpp`: getBusNumbers, getBusName, addBus, getSendToBusDb, setSendToBusDb, duckFromTrack, removeDucking, getDuckSource
- `EditSessionSampler.cpp`: getKngSampler, setBuiltInSampler, setSamplerSample, getSamplerSampleName, getSamplerRoot, setSamplerRoot, getSamplerGain, setSamplerGain, getSamplerOneShot, setSamplerOneShot, chopClipToPads, openPianoRollForTrack
- `EditSessionSelection.cpp`: clearSelection, toggleClipSelection, addClipsToSelection, isClipSelected, getSelectedClips, selectAllClips
- `EditSessionSynth.cpp`: setBuiltInSynth
- `EditSessionTempo.cpp`: barStartAt, barsAfter, barNumberAt, getBpmAt, setBpmAt, getTimeSigAt, addTempoChange, addTimeSigChange, getTempoMarks, removeTempoMark, setMidiLearn, isMidiLearn
- `EditSessionTools.cpp`: getBuiltInEffects, addBuiltInPlugin, bounceTrackInPlace, movePatternBlock
- `EditSessionTracks.cpp`: moveTrack, duplicateSelectedClip, copySelectedClip, pasteClip
- `EditSessionTransport.cpp`: play, record, stop, setBpm, getBpm, setClickEnabled, isClickEnabled, setCountInBars, getCountInBars, setLooping, isLooping, setLoopRange, getLoopStart, getLoopEnd, undo, redo, retake
- `EditSessionView.cpp`: setView, zoomAround, scrollBy, xToTime, timeToX, snapTime, getSnapGrid, setSnapGrid, beginEdit, splitSelectedClipAtPlayhead
- `EditSessionVolume.cpp`: getTrackVolumeDb, setTrackVolumeDb, getMasterVolumeDb, setMasterVolumeDb

## 5. Adding a KNG effect checklist
- Plugin .h/.cpp on KngFxBase, with params, presets, xmlTypeName, getName and createEditor
- View .h/.cpp on KngFaceView
- Add both .cpp files to CMakeLists.txt
- Add createBuiltInType<...>() in MainComponent.cpp
- Add { name, Plugin::xmlTypeName } to getBuiltInEffects() in EditSessionTools.cpp
- Add its colour and chip to kngIdentityFor() in InspectorCards.cpp

## 6. Saved properties
ValueTree properties the code saves. `<n>` is a pattern number.

On the edit (song):
- `kngPatternCount`: how many patterns the song has.
- `kngPatternName<n>`, `kngPatternColour<n>`: pattern n's name and colour.
- `kngPatternBars<n>`: pattern n's length in bars (1 to 4).
- `kngPatternSwing<n>`: pattern n's swing, in percent.
- `kngPatternHome<n>`: pattern n's home position, in seconds.
- `kngSnapGrid`: snap grid size, in grid lines per beat.
- `kngLaneHeight`: Playlist row height in pixels.
- `kngViewStart`, `kngViewLength`: the visible time range, in seconds.
- `kngFolderCollapsed_<hash>`: whether a folder is collapsed, keyed by a hash of its name.

On a track:
- `kngChannel`: true for a step-sequenced Rack channel.
- `kngStash<n>`: pattern n's clip for this track, kept as XML.
- `kngDrum`: marks a drum track. `kngTrackType`: "midi" for a MIDI track.
- `kngFolder`: the track's folder name.
- `kngAutoParam`, `kngAutoShown`: which parameter the automation row edits, and whether it shows.
- `kngRackOpen`: whether the track's Rack row is open.

On a clip:
- `kngPattern`: the pattern number the clip belongs to.
- `kngLane`: id of the Playlist lane the clip's block sits in.
- `kngSwing`: swing percent, copied from the clip's pattern.
- `kngKey`, `kngScale`, `kngChordRoot`, `kngChordType`, `kngStamp`, `kngTheoryOpen`: piano roll theory settings.

On a plugin:
- `kngBoost`: marks the hidden volume-boost plugin (skipped in effect lists).
- `kngDuck`: marks a compressor added for sidechain ducking.
- KNG Sampler: `kngFile`, `kngRoot`, `kngGain`, `kngAttack`, `kngRelease`, `kngGlide`, `kngMono`, `kngOneShot`, `kngLoop`, `kngLoopStart`, `kngLoopEnd`.
