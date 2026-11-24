# KNG Studio map

## 1. Project
KNG Studio, a DAW. C++20, JUCE 8, Tracktion Engine, Windows, CMake.
Built with menu.bat (option 2 builds and runs, option 7 commits and pushes).
Fonts are compiled in through juce_add_binary_data (KNGStudioAssets).
All source files are in Source/.
The app icon is Assets/Icon/KngIcon.png (ICON_BIG and ICON_SMALL in juce_add_gui_app).

## 2. Working rules
- Change only what a prompt asks for. Do not refactor or tidy anything else.
- Do not run bash, builds, type checks or servers unless a prompt asks.
- When you create or delete a source file, add or remove its line in this map in the same change.

## 3. File map

### App shell and windows
- `Main.cpp`: app entry point and main window.
- `MainComponent.cpp/.h`: main window layout; registers the built-in plugin types with the engine.
- `MainComponentProject.cpp`: project folders; open, close, new, save, restore backup, new-song setup, save as template, new song from template.
- `MainComponentMenus.cpp`: menu bar names, contents and command handling.
- `MainComponentKeys.cpp`: keyboard shortcuts, shortcuts help, play/stop, zoom to fit and the computer keyboard piano (Ctrl+K, letter keys play the selected instrument, Up/Down change octave).
- `MainComponentWindows.cpp`: dock panel openers and dialog windows.
- `StartScreen.cpp/.h`: opening screen: start something new, start from a template, or reopen a song.
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
- `EditSessionCrossfade.cpp`: crossfades overlapping audio clips on a lane.
- `EditSessionDrums.cpp`: drum tracks, pads, kit folders, creating pattern clips.
- `EditSessionExport.cpp`: mix, stem and multi-track export; render settings.
- `EditSessionExtras.cpp`: punch recording, effect chain templates, KNG Tune.
- `EditSessionFolders.cpp`: track folders: membership, collapse, rename, mute, solo.
- `EditSessionInstruments.cpp`: puts a built-in instrument on a track.
- `EditSessionLatency.cpp`: recording latency offset.
- `EditSessionMasterChain.cpp`: one-click master chain (Width, Comp, Air, Limiter)
- `EditSessionMidi.cpp`: MIDI tracks, MIDI clips, VST instruments, and live MIDI routing (the selected instrument track plays from a MIDI keyboard or pad, including one plugged in while the Studio is open, and playLiveNote for the computer keyboard piano).
- `EditSessionMixer.cpp`: effect list add/move/remove, reverb send and bus, master meters.
- `EditSessionNoteTools.cpp`: humanize, arpeggiate chords and note length for a pattern.
- `EditSessionPatternDelete.cpp`: deletes a pattern and renumbers the ones after it.
- `EditSessionPatternInfo.cpp`: pattern name, colour, home, bank start, song end, duplicate.
- `EditSessionPatternLane.cpp`: lists pattern blocks, removes a block, restores stashed clips.
- `EditSessionPatternRecord.cpp`: folds notes recorded in Pattern mode into the pattern.
- `EditSessionPatterns.cpp`: pattern count, bars, steps, swing, clips, place and loop.
- `EditSessionPlaylist.cpp`: lanes; placing, moving and deleting blocks; cue and pattern mode.
- `EditSessionPlaylistEdit.cpp`: cut and resize blocks; tidy the Playlist; separate overlaps.
- `EditSessionPlugins.cpp`: adds instruments and effects by id (built-in or VST).
- `EditSessionProject.cpp`: save, import audio, copy files into the project.
- `EditSessionRouting.cpp`: buses, sends to a bus (level, pre/post fader, mute, remove), loop-safe routing check (canFeed, getBusTargets), sidechain ducking. Output itself lives in RouteMenu.h (KngRoute).
- `EditSessionSampler.cpp`: KNG Sampler settings, chop a clip to pads, open the piano roll.
- `EditSessionSelection.cpp`: multi-clip selection.
- `EditSessionSlices.cpp`: Slices: findHits (finds hits in a clip), writeSliceFile, chopClipAtCuts (Rack rows plus Chops), slicesToPads (Drum Kit pads)
- `EditSessionSynth.cpp`: puts KNG Synth on a track.
- `EditSessionTempo.cpp`: bar maths, tempo and time-signature changes, MIDI learn.
- `EditSessionTools.cpp`: built-in effects list, adding one, bounce in place, move pattern block.
- `EditSessionTracks.cpp`: move a track; duplicate, copy and paste a clip.
- `EditSessionTransport.cpp`: play, record, stop, BPM, click, count-in, loop, undo/redo, retake.
- `EditSessionView.cpp`: view range, zoom, scroll, x/time mapping, snap grid, beginEdit, split.
- `EditSessionVocalAlign.cpp`: lines a double or ad-lib up with the lead vocal (slide, then per-phrase nudges)
- `EditSessionVolume.cpp`: track and master volume in dB.

### Playlist
- `TracksView.cpp/.h`: the Playlist: one row per lane; handles plugin and pattern drops.
- `TrackHeader.cpp/.h`: a lane's header: colour strip, name, R M S (plus Mon while the lane is armed), meter, input.
- `TrackLane.cpp/.h`: one lane's clip area.
- `TrackLaneMouse.cpp`: TrackLane hit testing and the click, drag, release path.
- `TrackLaneMouseActions.cpp`: TrackLane scroll wheel, keys, clip menu, audio file drops, double-click.
- `BlockLayer.cpp/.h`: sits over one lane and draws its pattern blocks.
- `PatternWav.h`: header-only: KngPatternWav::render renders a pattern block to a WAV loop in the Kits folder under My Loops, named like "Hats 140bpm.wav" (block menu: Convert to WAV).
- `PitchTracker.h` (header only): finds the notes in one melody line (YIN pitch guesses every 10 ms, steady stretches become notes). One note at a time, no chords. Used by Convert to MIDI.
- `AudioToMidi.h` (header only): Convert to MIDI. Runs the pitch tracker on an audio clip and puts the notes in a MIDI clip on a new KNG Synth track.
- `PlaylistScrollBar.h` (header only): the horizontal scroll bar under the Playlist lanes. It covers the song plus one screen of empty bars.
- `AutomationLane.cpp/.h`: automation row: AutomationHeader (pick target) and the curve.
- `FolderHeader.cpp/.h`: bar above a folder's tracks: collapse, name, mute, solo.
- `TimelineRuler.cpp/.h`: the bar.beat ruler above the lanes.
- `MarkerBar.cpp/.h`: thin row above the ruler for song section markers.
- `PlaylistToolbar.h`: Select, Paint and Cut tools; keeps the Playlist tidy.
- `ClipProps.cpp/.h`: stretch, pitch and gain for one audio clip.
- `SliceEditorView.cpp/.h`: Slice Editor window: waveform with draggable slice markers, Detect Hits, Chop to Rack, Send to Pads. showSliceEditor (session, clip) opens it. Opened from the audio clip right-click menu.
- `CompView.cpp/.h`: take comping for an audio clip, and its window.

### Rack and patterns
- `RackView.cpp/.h`: every sound and instrument in one place, editing the current pattern. Steps keep a minimum width of 20 px and long patterns scroll sideways. The Bars dropdown is gone: the pattern grows when a step is placed in a locked bar, and the Trim button shortens it.
- `RackRow.cpp/.h`: one sound in the Rack: controls on the left, its pattern right. Every bar with no step or note is dimmed (bar 1 never is), and dimmed bars past the pattern's end are locked. Placing a step in a locked bar grows the pattern (RackRow::growToStep). Erasing steps shrinks the pattern back (RackRow::trimToContent).
- `RackRowPaint.cpp`: RackRow painting (drum rows, mini piano roll) and header layout.
- `RackRowMouse.cpp`: RackRow mouse: row clicks, step painting, dragging the pattern out.
- `RackRowActions.cpp`: RackRow's actions and menus.
- `RackRowGraph.cpp`: the step graph under a Rack row (velocity, or pitch on sound rows)
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
- `MixerViewInserts.cpp`: MixerStrip insert slots, opening plugins, add-effect menu.
- `MasterStrip.cpp/.h`: the master channel strip.
- `LevelMeter.cpp/.h`: level meter component.
- `LoudnessDisplay.cpp/.h`: master loudness readout: momentary, short-term, integrated LUFS, peak.
- `KngLoudnessPlugin.cpp/.h`: invisible BS.1770 loudness meter at the end of the master chain.
- `RoutePopup.h`: Routing pop-up for a lane: Output (Master or a bus), and its sends with level, Pre/Post fader, mute, remove.
- `FxChainComponent.cpp/.h`: a track's effect chain list with its add menu, and its window.
- `SidechainMenu.h`: header-only: KngSidechain::addMenu adds a "Sidechain" source submenu to a plugin slot's menu.
- `RouteMenu.h`: header-only: KngRoute::addItems / addMenu list where a track's output can go (Master or another track), with loop protection.
- `TrackWidth.h`: header-only: KngTrackWidth::get / set drive a hidden KNG Width stage after a track's volume (the Width control).
- `VocalChain.h`: header-only: KngVocalChain::apply puts the one-click vocal chain (Gate, EQ, Comp, De-Esser, Air) on a track.
- `ProjectTemplates.h`: header-only: song templates (a saved song without its audio clips) in Documents\KNG Studio\ProjectTemplates as .kngtemplate files.
- `DockPanel.cpp/.h`: tabbed panel docked along the bottom, plus its resize divider.
- `TrackInspector.cpp/.h`: right-side panel: selected track's volume, instrument, effects, sends.
- `InspectorCards.cpp/.h`: KngIdentity (colour and chip per plugin, kngIdentityFor) and InstrumentCard.
- `InspectorParts.cpp/.h`: ParamKnob, EffectRow and a plugin's most useful knobs.
- `VolumeHelpers.h`: shared volume fader range: -inf to +12 dB, double-click for 0.

### Built-in instruments
- `Kng808Plugin.cpp/.h`: synthesized 808: pitch drop, length, punch, drive, glide.
- `Kng808View.cpp/.h`: KNG 808's face.
- `KngKeysPlugin.cpp/.h`, `KngKeysView.cpp/.h`: KNG Keys, synthesized piano, Rhodes and organ (KngFxBase instrument).
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
- `KngFilterPlugin.cpp/.h`: resonant sweep filter with tempo-synced wobble (KngFxBase).
- `KngFilterView.cpp/.h`: KNG Filter's face: filter curve riding the wobble.
- `KngModPlugin.cpp/.h`: chorus, flanger and phaser (KngFxBase).
- `KngModView.cpp/.h`: KNG Mod's face: left and right waves swaying with the sweep.
- `KngLoFiPlugin.cpp/.h`: bits, rate, tape wobble, dust and tone (KngFxBase).
- `KngLoFiView.cpp/.h`: KNG Lo-Fi's face: cassette with spinning reels.
- `KngPumpPlugin.cpp/.h`: tempo-synced sidechain-style volume pumping (KngFxBase).
- `KngPumpView.cpp/.h`: KNG Pump's face: volume curve with a dot riding it in time.
- `KngTransientPlugin.cpp/.h`: attack and sustain shaper (KngFxBase).
- `KngTransientView.cpp/.h`: KNG Transient's face: sample hit reshaped, over a live history.

### Shared UI (faces, look and feel, fonts, pop-outs)
- `KngFaceView.cpp/.h`: shared frame every KNG effect face is built on; kngMakeStyle.
- `KngFace.cpp/.h`: KngFaceStyle, KngFaceLook, KngFaceKnob, KngFaceToggle, KngFaceChoice, KngPopButton.
- `KngFxView.cpp/.h`: ready-made panel for KngFxBase effects, built from their parameters.
- `GenericPluginPanel.cpp/.h`: panel of sliders for plugins with no UI of their own.
- `KngLookAndFeel.h`: the app's one dark theme: buttons, knobs, sliders.
- `KngTheme.h`: Flat Dark color tokens (surfaces, text, accent, lane colors).
- `KngLogo.h`: header-only: the built-in KNG Studio logo picture, with its black background keyed out.
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
- `EditSession.cpp`: EditSession, ~EditSession, syncTrackState, getNumTracks, getTrack, addTrack, deleteTrack, getInputsFor, getInputNames, getTrackInput, setTrackInput, isArmed, isAnyTrackArmed, setArmed, applyArm, refreshInputs, setMonitoring, isTrackMonitor, setTrackMonitor, updateMonitoring, selectClip, deleteSelectedClip
- `EditSessionAutomation.cpp`: getAutomationTargets, getAutomationParam, getAutomationParamId, setAutomationParamId, isAutomationShown, setAutomationShown, setAutomationWrite, isAutomationWrite
- `EditSessionBackup.cpp`: getLockFile, getBackupsFolder, timerCallback, makeBackup, pruneBackups
- `EditSessionChannels.cpp`: addSampleChannel
- `EditSessionClips.cpp`: getClipSourceBpm, setClipFollowTempo, setClipPitch, setClipGain, getMarkers, addMarkerAt, jumpToMarker
- `EditSessionCrossfade.cpp`: crossfadeOverlaps
- `EditSessionDrums.cpp`: getKitsFolder, isDrumTrack, getDrumSampler, getPadName, setPadName, setPadSample, clearPad, getPadGain, getPadPan, setPadGainAndPan, getPadPitch, setPadPitch, playPad, loadKitFolder, createPatternClip
- `EditSessionExport.cpp`: getLameExecutable, getExportsFolder, trackHasContent, getExportRange, makeRenderParams, exportMix, exportStem, exportTracks
- `EditSessionExtras.cpp`: setPunchEnabled, isPunchEnabled, getFirstFxIndex, getTemplatesFolder, getTemplateNames, saveChainTemplate, loadChainTemplate, addKngTune
- `EditSessionFolders.cpp`: getTrackFolder, getFolderTracks, getFolderNames, setTrackFolder, isFolderCollapsed, setFolderCollapsed, renameFolder, removeFolder, toggleFolderMute, toggleFolderSolo
- `EditSessionInstruments.cpp`: setBuiltInInstrument
- `EditSessionLatency.cpp`: getLatencyOffsetMs, setLatencyOffsetMs
- `EditSessionMasterChain.cpp`: applyMasterChain
- `EditSessionMidi.cpp`: addMidiTrack, isMidiTrack, createMidiClip, setInstrument, getInstrument, removeInstrument, routeLiveMidi, refreshMidiInputs, playLiveNote
- `EditSessionMixer.cpp`: getFxPlugins, addPlugin, movePlugin, removePlugin, findSend, isReverbBus, getSendDb, setSendDb, ensureReverbBus, getMasterMeterPlugin, getLoudnessMeter
- `EditSessionNoteTools.cpp`: humanizePattern, arpeggiatePattern, setPatternNoteLength, humanizeClip, arpeggiateClip, setClipNoteLength
- `EditSessionPatternDelete.cpp`: deletePattern
- `EditSessionPatternInfo.cpp`: getBankStart, getPatternHome, getSongEnd, getPatternName, setPatternName, getPatternColour, setPatternColour, duplicatePattern
- `EditSessionPatternLane.cpp`: getPatternBlocks, removePatternAt, restorePatternStash
- `EditSessionPatternRecord.cpp`: mergePatternTake
- `EditSessionPatterns.cpp`: getBarSeconds, getNumPatterns, newPattern, getPatternBars, getPatternSteps, setPatternBars, getPatternClip, placePattern, loopPattern, makePatternUnique, getPatternSwing, setPatternSwing
- `EditSessionPlaylist.cpp`: isLaneTrack, getLaneId, getLaneTrackIndex, getDefaultLaneId, getBlockClips, placePatternInLane, moveBlock, deleteBlock, cuePattern, setPatternMode
- `EditSessionPlaylistEdit.cpp`: cutBlock, resizeBlock, tidyPlaylist, separateOverlappingBlocks
- `EditSessionPlugins.cpp`: setInstrumentById, addInstrumentTrack, addEffectById
- `EditSessionProject.cpp`: save, importAudioFile, copyIntoProject, importAudioFileAt
- `EditSessionRouting.cpp`: getBusNumbers, getBusName, addBus, getSendToBusDb, setSendToBusDb, duckFromTrack, removeDucking, getDuckSource, getSendBuses, isSendPreFader, setSendPreFader, isSendMuted, setSendMuted, removeSend, getBusTargets, getBusSources, canFeed
- `EditSessionSampler.cpp`: getKngSampler, setBuiltInSampler, setSamplerSample, getSamplerSampleName, getSamplerRoot, setSamplerRoot, getSamplerGain, setSamplerGain, getSamplerOneShot, setSamplerOneShot, chopClipToPads, openPianoRollForTrack
- `EditSessionSelection.cpp`: clearSelection, toggleClipSelection, addClipsToSelection, isClipSelected, getSelectedClips, selectAllClips
- Slice functions (findHits, chopClipAtCuts, slicesToPads, writeSliceFile): EditSessionSlices.cpp. chopClipToPads (EditSessionSampler.cpp) now only builds the cut list and calls chopClipAtCuts.
- `EditSessionSynth.cpp`: setBuiltInSynth
- `EditSessionTempo.cpp`: barStartAt, barsAfter, barNumberAt, getBpmAt, setBpmAt, getTimeSigAt, addTempoChange, addTimeSigChange, getTempoMarks, removeTempoMark, setMidiLearn, isMidiLearn
- `EditSessionTools.cpp`: getBuiltInEffects, addBuiltInPlugin, bounceTrackInPlace, movePatternBlock
- `EditSessionTracks.cpp`: moveTrack, duplicateSelectedClip, copySelectedClip, pasteClip
- `EditSessionTransport.cpp`: play, record, stop, setBpm, getBpm, setClickEnabled, isClickEnabled, setCountInBars, getCountInBars, setLooping, isLooping, setLoopRange, getLoopStart, getLoopEnd, undo, redo, retake
- `EditSessionView.cpp`: setView, zoomAround, scrollBy, xToTime, timeToX, snapTime, getSnapGrid, setSnapGrid, beginEdit, splitSelectedClipAtPlayhead, getPlaylistEnd
- `EditSessionVocalAlign.cpp`: alignClipToTrack
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
- `kngPatternBars<n>`: pattern n's length in bars (1 to 32, new patterns start at 1). Bank slots are 32 bars apart; older projects are moved to the 32 bar slots once (flag `kngBankSlotBars`).
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
- `kngGraph`: which step graph a Rack row shows: 0 off, 1 velocity, 2 pitch.
- `kngGraphPad`: which kit pad the step graph edits.

On a clip:
- `kngPattern`: the pattern number the clip belongs to.
- `kngLane`: id of the Playlist lane the clip's block sits in.
- `kngSwing`: swing percent, copied from the clip's pattern.
- `kngKey`, `kngScale`, `kngChordRoot`, `kngChordType`, `kngStamp`, `kngTheoryOpen`: piano roll theory settings.

On a plugin:
- `kngBoost`: marks the hidden volume-boost plugin (skipped in effect lists).
- `kngDuck`: marks a compressor added for sidechain ducking.
- `kngWidthStage`: marks the hidden KNG Width stage behind a track's Width control (skipped in effect lists).
- `kngVocalChain`: marks the plugins added by the Vocal Chain button (replaced when it is clicked again).
(no new saved property for templates; they are .kngtemplate XML files, see ProjectTemplates.h)
- KNG Sampler: `kngFile`, `kngRoot`, `kngGain`, `kngAttack`, `kngRelease`, `kngGlide`, `kngMono`, `kngOneShot`, `kngLoop`, `kngLoopStart`, `kngLoopEnd`.

## 7. To do
Planned work, roughly in order. When an item is finished, move it out of this list and update the file map above in the same change.

### Test first
- Build and test the Monitor, Automate and Routing batch: Mon turns on when a lane is armed; the Automate submenu, the x button and Delete this automation; the Routing pop-up from the lane, Mixer and Inspector; Output and loop prevention; moving a routed lane up or down, then playing.
- Run git rm Source/SendPopup.h (the file is no longer used).
- Test the computer keyboard piano (Ctrl+K) and a MIDI keyboard. Both are built and untested.

### Immediate, easiest to hardest
Small:
1. Tempo tap: a Tap button by the tempo display.
2. Count-in dropdown next to the metronome. EditSessionTransport already has setCountInBars and getCountInBars; add the dropdown in TransportBar if it is missing.
3. Record quantization chooser: pick a grid that recorded MIDI notes snap to.
4. Undo history list: a panel of undo steps; click one to step back to it.
5. Punch in/out: setPunchEnabled exists (EditSessionExtras); add draggable in and out points on the ruler.
6. Audio settings extras: buffer size, recording latency offset (EditSessionLatency and LatencyDialog exist), record folder and bit depth. Add whichever are missing to SettingsComponent.

Medium:
7. Info panel: a hint bar at the bottom of the window first, then a floating hover box.
8. Make Unique and Group Clips for Playlist clips (patterns already have makePatternUnique).
9. Routing matrix: a grid of what feeds what (getBusTargets and canFeed already hold the data).
10. Per-plugin CPU monitor: double-click the CPU meter. Check first what the engine reports per plugin.
11. Saved window layouts.
12. Stem export per lane: ExportDialog, exportStem and exportTracks exist. Make sure one click exports every lane.
13. Slice Editor, first part: Grid cuts (with triplets), Split into N, tap-in cuts during playback, and per-slice reverse, pitch and volume.

Large:
14. Master output panel: LoudnessDisplay already shows LUFS and peak. Add a scope and a spectrum.
15. Sections: song markers exist (MarkerBar, getMarkers, addMarkerAt, jumpToMarker). Make them draggable named blocks with a length. Moving a block moves everything under it. One feature, not two.
16. Collect and Save: importAudioFile and copyIntoProject already copy imports into the project. Extend it to Sounds browser files, KNG Sampler files and Kit samples, with a File menu item.
17. Slice Editor, second part: tempo match before cutting, Rearrange on Playlist (slices become clips), Snap to key (KngTune and the theory settings), and opening the editor from a lane.

### Waiting on a decision
- Comping: CompView exists. Decide on take lanes under the lane and pitch scoring with KngTune.
- Clip gain line with Auto Ride (per-clip gain exists; the line is the parked Gain envelopes item below).
- Clean Takes: strip silence that keeps breaths.
- Capture: an always-listening buffer for audio and MIDI, per lane.
- Stem separation (needs a model licence check first).
- Warp markers with three plainly named modes (tempo follow already exists).
- Scratch Pads and Swap Clips.
- Edit menu: Cut Clip, Select None, Invert Selection. File menu: Recent Projects, Project Settings. Fold Audio Settings, Scan Plugins and Latency Check into tabs inside Settings.
- Duplicate Lane. Unified search across plugins, samples and projects.

### Earlier ideas still open
- Sends chips row in the lane header on tall lanes (about 82 px and up).
- Thin out automation points after a Write pass.
- Learn moved to a right-click on knobs.
- Per-lane automation mode (check the engine API first).
- Output menu for instrument tracks in the Rack.
- Inspector plugin colour chip: keep it or switch to a tick.
- Playhead follow on long Rack patterns; pin Rack row controls while scrolling.
- Piano roll rows that auto-grow; piano roll following the transport Snap button.
- Known quirks: hiding an automation row with x leaves the curve playing; each lane automates one parameter at a time; the Write switch is global; a new song has no bus until "New bus..." is used.

### Parked
- KNG Vox Sat, time shaper and transient widener (premium plugins in the KngPlugins repo).
- AI chat helpers, Link Audio, ARA 2 and Session-style clip launching.

### Plugin direction
- Stock effect faces: done. Every stock effect has its own face. Only revisit one if it looks off.
- The KNG Vox plugins (KngPlugins repo: Vox Comp, Vox DeEss, Vox Void, Vox EQ) are the premium line, possibly sold later. They stay standalone VST3s and are not ported into the built-ins.
- Confirm the Vox VST3s scan, load in a track slot, save and reload with the project, and show up in the Plugin Browser.
- KngSat stays a single-band saturator. Multiband saturation is a new premium plugin in the Vox line (KNG Vox Sat), built in the KngPlugins repo.
- Gross Beat style time shaper (tempo-locked stutter, gate, half-speed and reverse patterns) is a premium plugin in the KngPlugins repo, not a Studio built-in. KNG Pump stays as the Studio's stock version of the idea.
- Transient widener (widens the stereo image of hits and sustain separately) is also a premium plugin in the KngPlugins repo, not a Studio built-in. KNG Width and KNG Transient stay as the stock versions.

### Mixing and routing
- Sidechain (built into mixer inserts, FX chain rows and inspector rows, not yet tested): a "Sidechain: [track]" source choice on each plugin slot (FxChainComponent, MixerViewInserts, TrackInspector) so any plugin with a sidechain input can be fed by another track. Needed so KNG Vox EQ can duck a beat from the vocal. Today EditSessionRouting only adds a KNG Comp marked kngDuck. First check how Tracktion Engine exposes sidechain sources.
- Buses and routes: built (RoutePopup opens from the lane header Route button, the Mixer strip Sends button and the Inspector; Output goes through KngRoute), not yet tested.
- Output menu: built into the inspector, the mixer strips and the Routing pop-up, not yet tested. Moving a routed lane up or down, then playing, must be tested.
- Track widener (built into the inspector and mixer strips, not yet tested): a stereo width control on every track.

### Vocals
- Vocal chain template (built into the inspector and mixer "+ FX" menus, not yet tested; the EQ has a low cut at 80 Hz and the rest is flat): one click adds a ready vocal chain (De-Ess, EQ, Comp, Air, Gate) to a track, built on the existing effect chain templates and master chain code.

### Audio and editing
- Gain envelopes: draw volume points inside an audio clip. Clip fades and per-clip gain already exist (TrackLane), and track volume automation exists (AutomationLane). Tracktion's clip-level volume is a ClipEffect that renders to a cache file, so it is slow to edit. Parked until we know the automation lanes are not enough.
- Play any sound from a MIDI keyboard or the computer keyboard (MIDI keyboard and pad part built (picks up a device plugged in while the Studio is open), not yet tested; computer keyboard piano built (Ctrl+K, Up/Down for octave, not recorded into clips yet), not yet tested): whichever track or Rack row is selected plays from the controller with no setup (no arming and no picking an input), and can record notes into the piano roll or pattern.
- Audio to MIDI: right-click an audio clip > Convert to MIDI. A single-voice pitch tracker (a hummed or sung melody, a bass line) makes a MIDI clip on a new track. Not for chords or full mixes. Built: right-click an audio clip, Convert to MIDI (one voice or melody only, makes a new KNG Synth track). Not yet tested.
- Wav Converter (built: right-click a pattern block in a lane > Convert to WAV, saves to Kits, My Loops, so the loops show up in the Sounds browser, not yet tested): turn a pattern into a WAV loop.

### Project
- Project templates (built: File > Save as Template and the Start Screen "From Template..." button, not yet tested): save a whole song setup (tracks, lanes, instruments, effects, tempo) as a template and start a new song from one on the Start Screen.
- Drag sounds from the Sounds browser onto Playlist lanes: built (audio lands on the lane at the drop point, below the lanes makes a new lane), not yet tested.
- Reorder effects: drag effects up and down in a mixer strip (built, not yet tested), plus Move Up / Move Down in the mixer insert right-click menu and on the inspector rows. Dragging in the inspector list is still to do.
- Mixer strips with more than four effects: the fourth slot becomes a "+N more" button that lists the rest (built, not yet tested).
- Playlist scroll bar for more bars: bar built (covers the song plus one empty screen), not yet tested.
- Bars past the end of the song are dimmed in the Playlist lanes until something is placed there: built, not yet tested.
- Snap on and off in every area. The Playlist follows the Snap button already, the piano roll has its own grid box with Grid Off. Check drops and anywhere else that ignores it.