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
- `MainComponentKeys.cpp`: keyboard shortcuts, shortcuts help, play/stop, zoom to fit and the computer keyboard piano (Ctrl+K, letter keys play the selected instrument, Up/Down change octave). Ctrl+1/2/3 load window layout 1 to 3, Ctrl+Shift+1/2/3 save the current layout into that slot.
- `MainComponentWindows.cpp`: dock panel openers, dialog windows and saved window layouts (saveLayout, loadLayout: three slots in Documents\KNG Studio\layouts.xml, menu ids 9101 to 9103 and 9201 to 9203 live in MainComponentMenus.cpp).
- `StartScreen.cpp/.h`: opening screen: start something new, start from a template, or reopen a song.
- `RecentProjects.h`: header-only: KngRecent::list gives the newest songs in Documents\KNG Studio for File > Recent Projects.
- `HintBar.h` (header only): the hint text in the middle of the status bar. It shows the tooltip of whatever the mouse is over, 10 times a second.
- `SettingsComponent.cpp/.h`: settings for audio, plugins, recording (including the latency check), folders and about. It is the one door: the separate Audio Settings, Scan Plugins and Latency Check menu items are gone.
- `TransportBar.cpp/.h`: top bar with transport, tempo and song controls.
- `ExportDialog.cpp/.h`: export dialog for the mix and stems.
- `LatencyDialog.cpp/.h`: dialog for measuring and setting recording latency.
- `ProjectSettings.h`: header-only: File > Project Settings window (count-in, record quantize, snap grid, click, snap, open song folder, backups).
- `ScratchPads.h`: header-only: View > Scratch Pads window, text pads saved with the song.
- `CleanTakes.h`: header-only: Clean Takes (Edit menu and the comp window button). Deletes the takes a clip is not using, after a confirm box that also asks about deleting the audio files.
- `LatencyTester.cpp/.h`: plays a click and times how long it takes to come back in.
- `MidiMapDialog.cpp/.h`: lists every MIDI controller link in the song, with Remove.
- `UndoHistory.h` (header only): Edit > Undo History window (UndoHistoryList::open): redo steps, Now, undo steps; click one to jump to it. Its menu id cmdUndoHistory (9001) lives in MainComponentMenus.cpp.
- `PluginWindow.cpp/.h`: plugin editor windows, and KngUIBehaviour that hooks them into the engine.
- `PluginBrowser.cpp/.h`: every instrument and effect in one searchable list.
- `KitBrowser.cpp/.h`: left-side Sounds panel (kits, search, favorites) and SamplePreviewer.

### EditSession (the song model)
- `EditSession.cpp/.h`: the class itself; construction, tracks, inputs, arming, monitoring, clip select/delete.
- `EditSessionAutomation.cpp`: automation targets and per-track shown/write state.
- `EditSessionBackup.cpp`: lock file, timed backups and pruning old ones.
- `EditSessionChannels.cpp`: adds a sample channel to the Rack.
- `EditSessionClips.cpp`: audio clip tempo-follow, pitch and gain; song markers.
- `EditSessionClipTools.cpp`: cut clip, invert selection, swap two selected clips, duplicate lane (audio clips and effects; pattern blocks are not copied). Select None is clearSelection.
- `EditSessionSections.cpp`: Sections (a marker with a length and a kngSection property): addSection, getSectionClips, moveClipsInRange, duplicateSection (shifts everything after the section right by its length, copies the clips, adds a new numbered section).
- `EditSessionCollect.cpp`: collectAndSave: copies audio clips and Drum Kit pad samples that live outside the project into its Audio folder and points the song at the copies (File > Collect and Save...). It also copies the KNG Sampler's sample (the kngFile property) on any lane.
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
- `EditSessionProject.cpp`: save, import audio, copy files into the project. While batchImport is true importAudioFileAt and save() skip their own undo step and save (Chop to Lane sets it, so one Undo takes back the whole chop).
- `EditSessionRouting.cpp`: buses, sends to a bus (level, pre/post fader, mute, remove), loop-safe routing check (canFeed, getBusTargets), sidechain ducking. Output itself lives in RouteMenu.h (KngRoute).
- `EditSessionSampler.cpp`: KNG Sampler settings, chop a clip to pads, open the piano roll.
- `EditSessionSelection.cpp`: multi-clip selection and Group Clips (clips with the same kngGroup property are picked, moved, copied and deleted together; groups expand inside getSelectedClips and isClipSelected; duplicates and pastes get new ids through regroupCopies).
- `SliceFx.h`: SliceFx (reverse, pitch in semitones, volume in dB for one slice) and applySliceFx. Header only. EditSession::setSliceFx hands the list to the next chop or send to pads.
- `EditSessionSliceLane.cpp`: chopClipToLane. Writes each slice to a file and puts each one on a new audio lane as its own clip (original spot, or on the song's tempo when setSliceSourceBpm is set), then mutes the original clip.
- `EditSessionSlices.cpp`: Slices: findHits (finds hits in a clip), writeSliceFile, chopClipAtCuts (Rack rows plus Chops), slicesToPads (Drum Kit pads)
- `EditSessionSynth.cpp`: puts KNG Synth on a track.
- `EditSessionTempo.cpp`: bar maths, tempo and time-signature changes, MIDI learn.
- `EditSessionTools.cpp`: built-in effects list, adding one, bounce in place, move pattern block.
- `EditSessionTracks.cpp`: move a track; duplicate, copy and paste a clip.
- `EditSessionTransport.cpp`: play, record, stop, BPM, click, count-in, record quantize, loop, undo/redo, retake.
- `EditSessionView.cpp`: view range, zoom, scroll, x/time mapping, snap grid, beginEdit, split.
- `EditSessionVocalAlign.cpp`: lines a double or ad-lib up with the lead vocal (slide, then per-phrase nudges)
- `EditSessionVolume.cpp`: track and master volume in dB.

### Playlist
- `TracksView.cpp/.h`: the Playlist: one row per lane; handles plugin and pattern drops.
- `TrackHeader.cpp/.h`: a lane's header: colour strip, name, R M S (plus Mon while the lane is armed), meter, input.
- `TrackLane.cpp/.h`: one lane's clip area.
- `GroupBadge.h`: header-only. KngGroupBadge::draw puts a small "G" tab on the name band of a grouped clip (TrackLane::drawClips calls it).
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
- `MarkerBar.cpp/.h`: thin row above the ruler. Markers show as a label (double-click to add). Sections show as a named block (right-click empty space to add): drag to move it with the clips that start inside it, drag the right edge to resize, right-click for Rename, Duplicate Section, Select Clips in Section and Delete.
- `PlaylistToolbar.h`: Select, Paint and Cut tools; keeps the Playlist tidy.
- `ClipProps.cpp/.h`: stretch, pitch and gain for one audio clip.
- `SliceEditorLane.cpp`: Slice Editor Chop to Lane button (setupLane, chopToLane). Members of SliceEditorView.
- `SliceEditorKey.cpp`: Snap to Key: Key and scale pickers plus a Snap to Key button. It reads each slice's audio, finds its note with SlicePitch.h and sets that slice's Pitch so it lands on the nearest note of the key. Slices with no clear note are skipped. Assumes the clip plays at its source speed.
- `SliceEditorTempo.cpp`: Slice Editor tempo match. Match tempo toggle, source BPM box and Detect (guesses from the clip length as 1 to 16 bars of 4/4). On Chop to Rack the editor calls EditSession::setSliceSourceBpm, so chopClipAtCuts scales the Chops clip and notes from the clip's tempo to the song's tempo. Members of SliceEditorView.
- `SliceEditorFx.cpp`: Slice Editor per-slice controls. Pick a slice (arrows or the number badge), then Reverse, Pitch (semitones) and Volume (dB). syncSliceFx keeps one SliceFx per slice as markers are added or removed. The settings are baked into the slice files on Chop to Rack and Send to Pads. Members of SliceEditorView.
- `SliceEditorTap.cpp`: Slice Editor tap-in cuts. Play button, Cut Here button and the T key add a cut at the red playhead. Space starts and stops. Members of SliceEditorView.
- `SlicePitch.h`: header-only. KngSlicePitch::detect (YIN pitch of a mono piece of audio, 60 Hz to 1 kHz, 0 if no clear note), semitonesToKey (how far to move it to reach the nearest note of a key and scale, rounded to 0.1) and the scale list (Major, Minor, Harmonic Minor, Pentatonic Major and Minor, Blues, Chromatic).
- Source/SlicePitchShift.h: offline pitch shift that keeps length (engine TimeStretcher, SoundTouch). applyFx() is the one place slice effects run. EditSession::sliceKeepLengthForNextWrite (default true) comes from the Slice Editor "Record-style pitch" checkbox (off = real pitch shift, on = old varispeed).
- `SliceEditorView.cpp/.h`: Slice Editor window: waveform with draggable slice markers, Detect Hits, Grid cut (1/4 to 1/32, with triplets), Split into N parts, Chop to Rack, Send to Pads. showSliceEditor (session, clip) opens it. Opened from the audio clip right-click menu, and from a lane header's right-click menu (a Slice Editor submenu that lists the lane's audio clips).
- `SliceCuts.h` (header only): cut lists for the Slice Editor: grid cuts on the tempo map (KngSliceCuts::grid) and equal parts (KngSliceCuts::equalParts).
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
- `MasterScope.h`: header-only. The master output drawn live: waveform (triggered on an upward zero crossing, left and right in two colours), a 40-band spectrum (4096-point FFT, tilted 3 dB per octave) and a left/right match figure in the big version. Small copy in MasterStrip (right of the meter). MasterScope::open (session) opens View > Master Output... and returns the new window (the caller keeps it in songWindows) or nullptr if one is already open. Reads samples with KngLoudnessPlugin::copyRecent.
- `LoudnessDisplay.cpp/.h`: master loudness readout: momentary, short-term, integrated LUFS, peak.
- `KngLoudnessPlugin.cpp/.h`: invisible BS.1770 loudness meter at the end of the master chain. It also keeps the newest 8192 samples of left and right in a lock-free ring (copyRecent, getScopeSampleRate) for MasterScope.
- `PluginCpuMonitor.h`: header-only. View > Plugin CPU Monitor... opens a window listing every plugin on every lane and on the master, busiest first, with a bar and a percent of the audio buffer (Plugin::getCpuUsage, smoothed). PluginCpuMonitor::open (session) returns the new window (the caller keeps it in songWindows) or nullptr if one is already open.
- `RoutingMatrix.h`: header-only. View > Routing Matrix... opens a window with one row per lane: an Output button (same menu as the Routing pop-up) and a send level cell for every bus. Greyed cells would make a loop. RoutingMatrix::open (session) returns the new window (the caller keeps it in songWindows) or nullptr if one is already open.
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
- `EditSessionClipTools.cpp`: cutSelectedClip, invertSelection, swapSelectedClips, duplicateLane
- `EditSessionCollect.cpp`: collectAndSave
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
- `EditSessionSelection.cpp`: clearSelection, toggleClipSelection, addClipsToSelection, isClipSelected, getSelectedClips, isClipSelectedDirect, groupSelectedClips, ungroupSelectedClips, isClipGrouped, regroupCopies, selectAllClips
- Slice functions (findHits, chopClipAtCuts, slicesToPads, writeSliceFile): EditSessionSlices.cpp. chopClipToPads (EditSessionSampler.cpp) now only builds the cut list and calls chopClipAtCuts.
- `EditSessionSynth.cpp`: setBuiltInSynth
- `EditSessionTempo.cpp`: barStartAt, barsAfter, barNumberAt, getBpmAt, setBpmAt, getTimeSigAt, addTempoChange, addTimeSigChange, getTempoMarks, removeTempoMark, setMidiLearn, isMidiLearn
- `EditSessionTools.cpp`: getBuiltInEffects, addBuiltInPlugin, bounceTrackInPlace, movePatternBlock
- `EditSessionTracks.cpp`: moveTrack, duplicateSelectedClip, copySelectedClip, pasteClip
- `EditSessionTransport.cpp`: play, record, stop, setBpm, getBpm, setClickEnabled, isClickEnabled, setCountInBars, getCountInBars, setLooping, isLooping, setLoopRange, getLoopStart, getLoopEnd, getRecordQuantize, setRecordQuantize, quantiseRecordedTake, undo, redo, retake
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
- `kngRecQuant`: record quantize grid in beats (missing or 0 = off, 0.25 = 16th).
- `kngLaneHeight`: Playlist row height in pixels.
- `kngPads`: the Scratch Pads, as JSON text: a list of { name, text }.
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
- `kngGroup`: a group id shared by clips that belong together (Group Clips).
- `kngLane`: id of the Playlist lane the clip's block sits in.
- `kngSection` on a marker clip: true when the marker is a section block.
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
1. Tempo tap: built (Tap button next to the tempo box), not yet tested.
2. Count-in dropdown: already built (countInBox in TransportBar: none, 1 bar, 2 bars). Nothing to do.
3. Record quantization chooser: built (Quantize box after the Snap grid box; snaps note starts when recording stops, in Pattern mode and in normal takes), not yet tested.
4. Undo history list: built (Edit > Undo History...). Steps named so far: Split clip, Delete clip, Delete lane, Record quantize, Move lane, Duplicate clip, Paste clip, Add effect, Move effect, Remove effect, Copy block, Move block, Change instrument, Remove instrument; the rest show as "Edit N". Name more with beginEdit ("Name") or edit->getUndoManager().setCurrentTransactionName ("Name"). Not yet tested.
5. Punch in/out: the Punch button records only between the loop markers. Decide whether it needs its own in and out points, separate from the loop.
6. Audio settings extras: buffer size and sample rate are already in Settings > Audio (the JUCE device selector), and the recording latency offset has its own dialog. Still missing: recording bit depth and record folder. The Tracktion docs show no setting for either, so find the real one before building.

Medium:
7. Info panel: first half built (HintBar in the status bar shows the tooltip of whatever the mouse is over), not yet tested. Tooltips added to the controls that had none (AutomationHeader, FX chain Add, Sounds panel Open Folder and view box): done, not yet tested. Still to do: decide if a floating box is needed.
8. Group Clips: built and not yet tested (Ctrl+G groups, Ctrl+Shift+G ungroups, both are in the clip right-click menu; grouped clips show a small "G" badge (GroupBadge.h): done, not yet tested). Make Unique already exists for pattern blocks (block menu). Audio clips need nothing, because Duplicate makes an independent clip and edits never change the source file.
9. Routing matrix: built (View > Routing Matrix...), not yet tested.
10. Plugin CPU Monitor: built (View > Plugin CPU Monitor...), not yet tested. If every plugin shows 0% while playing, the engine flag TRACKTION_ENABLE_PLUGIN_CPU_MEASUREMENT may be off and must be set to 1 in target_compile_definitions.
11. Saved window layouts: built (View > Layouts; three slots; remembers panels, dock height and tab, browser tab), not yet tested. Ctrl+1/2/3 load, Ctrl+Shift+1/2/3 save (not tested).
12. Stem export: built (Export dialog: "Stems: one per track" with an All/None checklist, and "Stems: one per folder"), not yet tested.
13. Slice Editor, first part: Grid cut and Split into N are built (SliceCuts.h), and so are tap-in cuts during playback and per-slice reverse, pitch and volume (SliceEditorTap.cpp, SliceEditorFx.cpp). None of it is tested yet.

Large:
14. Master output panel: built (small scope and spectrum in the master strip, big window under View > Master Output...), not yet tested.
15. Sections: built and not yet tested (add, move with clips, resize, duplicate with room, select clips, delete).
16. Collect and Save: built for audio clips, Drum Kit pads and the KNG Sampler (File > Collect and Save...), not yet tested.
17. Slice Editor, second part: tempo match is built (SliceEditorTempo.cpp), not yet tested. Chop to Lane is built (each slice becomes its own clip on a new lane), not yet tested. Opening the editor from a lane is built (lane header right-click, Slice Editor submenu), not yet tested. Snap to Key is built (bottom row of the Slice Editor; the pitch slider now moves in 0.1 steps), not yet tested.

### Waiting on a decision
- Comping: CompView exists. Decide on take lanes under the lane and pitch scoring with KngTune.
- Clip gain line with Auto Ride (per-clip gain exists; the line is the parked Gain envelopes item below).
- Clean Takes: strip silence that keeps breaths.
- Capture: an always-listening buffer for audio and MIDI, per lane. The engine already has retrospective recording (InputDevice::updateRetrospectiveBufferLength and InputDevice::setRetrospectiveLock), so this is mostly UI.
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