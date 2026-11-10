#include "MainComponent.h"
#include "KngSamplerPlugin.h"
#include "KngSynthPlugin.h"
#include "KngEqPlugin.h"
#include "KngCompPlugin.h"
#include "KngVerbPlugin.h"
#include "KngDelayPlugin.h"
#include "KngSatPlugin.h"
#include "KngDeEssPlugin.h"
#include "KngLimiterPlugin.h"
#include "KngDoublerPlugin.h"

MainComponent::MainComponent (tracktion::Engine& engineToUse)
    : engine (engineToUse)
{
    engine.getPluginManager().createBuiltInType<KngSamplerPlugin>();
    engine.getPluginManager().createBuiltInType<KngSynthPlugin>();
    engine.getPluginManager().createBuiltInType<KngEqPlugin>();
    engine.getPluginManager().createBuiltInType<KngCompPlugin>();
    engine.getPluginManager().createBuiltInType<KngVerbPlugin>();
    engine.getPluginManager().createBuiltInType<KngDelayPlugin>();
    engine.getPluginManager().createBuiltInType<KngSatPlugin>();
    engine.getPluginManager().createBuiltInType<KngDeEssPlugin>();
    engine.getPluginManager().createBuiltInType<KngLimiterPlugin>();
    engine.getPluginManager().createBuiltInType<KngDoublerPlugin>();

    setWantsKeyboardFocus (true);

    menuBar.setModel (this);
    addAndMakeVisible (menuBar);

    for (auto* l : { &projectLabel, &deviceLabel })
    {
        addAndMakeVisible (l);
        l->setFont (juce::Font (juce::FontOptions (12.0f)));
        l->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    }

    projectLabel.setJustificationType (juce::Justification::centredLeft);
    deviceLabel.setJustificationType (juce::Justification::centredRight);

    kitBrowser = std::make_unique<KitBrowser> (engine.getDeviceManager().deviceManager);
    pluginBrowser = std::make_unique<PluginBrowser> (engine);

    const juce::Colour tabColour (0xff24282c);
    browserTabs.addTab ("Sounds", tabColour, kitBrowser.get(), false);
    browserTabs.addTab ("Plugins", tabColour, pluginBrowser.get(), false);
    browserTabs.setTabBarDepth (28);
    addAndMakeVisible (browserTabs);

    addAndMakeVisible (dock);
    addAndMakeVisible (dockDivider);

    dockDivider.onDragStart = [this] { dockDragStartHeight = dockHeight; };
    dockDivider.onDrag = [this] (int dy)
    {
        dockHeight = juce::jlimit (150, juce::jmax (150, (int) (getHeight() * 0.7f)), dockDragStartHeight - dy);
        resized();
    };

    tracksViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (tracksViewport);

    engine.getDeviceManager().deviceManager.addChangeListener (this);
    updateDeviceLabel();

    loadProject (getLastProjectFile());

    setSize (1280, 800);

    // Start screen on launch, on top of everything
    addAndMakeVisible (startScreen);
    startScreen.onContinue = [this] { pendingStartMode = 0; pendingBeatFile = juce::File(); hideStartScreen(); };

    startScreen.onOpenProject = [this] (const juce::File& editFile)
    {
        pendingStartMode = 0;
        pendingBeatFile = juce::File();

        // Open that project the same way File > Open does once a file is chosen
        loadProject (editFile);
        hideStartScreen();
    };

    // These finish later (a name prompt or file chooser), so loadProject() closes the start screen
    startScreen.onOpenOther = [this] { pendingStartMode = 0; pendingBeatFile = juce::File(); openProject(); };
    startScreen.onNewBeat = [this]
    {
        pendingBeatFile = juce::File();
        pendingStartMode = 1;
        newProject();
    };

    // Pick the beat file first, then name the song
    startScreen.onNewVocal = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Pick your beat",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.mp3;*.flac;*.aif;*.aiff;*.ogg");

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (! fc.getResult().existsAsFile())
                                      return;

                                  pendingBeatFile = fc.getResult();
                                  pendingStartMode = 2;
                                  newProject();
                              });
    };

    showStartScreen();
}

MainComponent::~MainComponent()
{
    menuBar.setModel (nullptr);
    engine.getDeviceManager().deviceManager.removeChangeListener (this);
    closeProject();
}

//==============================================================================
juce::File MainComponent::getProjectsRoot()
{
    auto root = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KNG Studio");
    root.createDirectory();
    return root;
}

juce::File MainComponent::getLastProjectFile()
{
    auto marker = getProjectsRoot().getChildFile ("last_project.txt");

    if (marker.existsAsFile())
    {
        const auto path = marker.loadFileAsString().trim();

        if (juce::File::isAbsolutePath (path))
        {
            juce::File f (path);

            if (f.existsAsFile())
                return f;
        }
    }

    return getProjectsRoot().getChildFile ("Untitled").getChildFile ("Untitled.tracktionedit");
}

void MainComponent::setLastProjectFile (const juce::File& editFile)
{
    getProjectsRoot().getChildFile ("last_project.txt").replaceWithText (editFile.getFullPathName());
}

bool MainComponent::isBusyRecording() const
{
    return session != nullptr && session->getTransport().isRecording();
}

//==============================================================================
void MainComponent::closeProject()
{
    dock.setSession (nullptr);
    if (pluginBrowser != nullptr)
        pluginBrowser->setSession (nullptr);
    fxWindows.clear();
    pianoRollWindows.clear();
    mixerWindow = nullptr;
    compWindows.clear();
    drumRackWindows.clear();
    samplerWindows.clear();
    tracksViewport.setViewedComponent (nullptr, false);
    tracksView = nullptr;
    masterStrip = nullptr;
    ruler = nullptr;
    markerBar = nullptr;
    patternLane = nullptr;
    transportBar = nullptr;
    session = nullptr;   // saves on the way out
}

void MainComponent::loadProject (const juce::File& editFile)
{
    closeProject();

    session = std::make_unique<EditSession> (engine, editFile);
    session->onShowFxChain   = [this] (tracktion::PluginList& list, const juce::String& title) { showFxChain (list, title); };
    session->onShowPianoRoll = [this] (tracktion::MidiClip& clip) { showPianoRoll (clip); };
    session->onShowComp      = [this] (tracktion::WaveAudioClip& clip) { showComp (clip); };
    session->onShowClipProps = [this] (tracktion::AudioClipBase& clip) { showClipProps (clip); };
    session->onShowDrumRack  = [this] (int trackIndex) { showDrumRack (trackIndex); };
    session->onShowSampler   = [this] (int trackIndex) { showSampler (trackIndex); };
    session->onShowPluginEditor = [this] (tracktion::Plugin& p) { dock.showEffect (p); openDock(); };
    dock.setSession (session.get());
    pluginBrowser->setSession (session.get());

    transportBar = std::make_unique<TransportBar> (*session);
    addAndMakeVisible (*transportBar);

    ruler = std::make_unique<TimelineRuler> (*session);
    addAndMakeVisible (*ruler);

    markerBar = std::make_unique<MarkerBar> (*session);
    addAndMakeVisible (*markerBar);

    patternLane = std::make_unique<PatternLane> (*session);
    addAndMakeVisible (*patternLane);

    masterStrip = std::make_unique<MasterStrip> (*session);
    addAndMakeVisible (*masterStrip);

    tracksView = std::make_unique<TracksView> (*session);
    tracksViewport.setViewedComponent (tracksView.get(), false);

    projectLabel.setText ("Project: " + session->getProjectName(), juce::dontSendNotification);
    setLastProjectFile (editFile);

    // Heads-up if the last session crashed
    if (session->didRecoverFromCrash())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Recovered",
            "KNG Studio didn't close cleanly last time. Everything up to the last autosave (within a minute) is here.\n\n"
            "If something looks wrong, use File > Restore Backup.");

    resized();

    // Opened from the start screen: close it and set the window up for what was picked
    if (startScreenVisible)
        hideStartScreen();

    if (pendingStartMode != 0)
        setUpNewSong();

    pendingStartMode = 0;
    pendingBpm = 0.0;
    pendingBeatFile = juce::File();
    resized();
}

void MainComponent::newProject()
{
    if (isBusyRecording())
        return;

    const auto defaultName = pendingBeatFile.existsAsFile() ? pendingBeatFile.getFileNameWithoutExtension()
                                                            : juce::String ("My Song");

    auto* w = new juce::AlertWindow ("New Song", "Name your song and set the tempo:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", defaultName, "Name");
    w->addTextEditor ("bpm", "140", "BPM");
    w->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result != 1)
        {
            // Cancelled: nothing should carry over to the next project that opens
            pendingStartMode = 0;
            pendingBpm = 0.0;
            pendingBeatFile = juce::File();
            return;
        }

        const double bpm = w->getTextEditorContents ("bpm").getDoubleValue();
        pendingBpm = bpm > 0.0 ? juce::jlimit (40.0, 300.0, bpm) : 140.0;

        // File > New and Ctrl+N set up a song too
        if (pendingStartMode == 0)
            pendingStartMode = 1;

        createProject (w->getTextEditorContents ("name"));
    }), true);
}

void MainComponent::createProject (const juce::String& name)
{
    auto safeName = juce::File::createLegalFileName (name.trim());

    if (safeName.isEmpty())
        safeName = "Untitled";

    auto folder = getProjectsRoot().getNonexistentChildFile (safeName, "", false);
    folder.createDirectory();

    loadProject (folder.getChildFile (folder.getFileName() + ".tracktionedit"));
}

void MainComponent::openProject()
{
    if (isBusyRecording())
        return;

    pendingStartMode = 0;
    pendingBpm = 0.0;
    pendingBeatFile = juce::File();

    chooser = std::make_unique<juce::FileChooser> ("Open Project", getProjectsRoot(), "*.tracktionedit");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();

                              if (file.existsAsFile())
                                  loadProject (file);
                          });
}

void MainComponent::saveProject()
{
    if (session != nullptr)
        session->save();
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Track", "Transport", "View", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int menuIndex, const juce::String&)
{
    juce::PopupMenu m;

    auto add = [&m] (int id, const juce::String& text, const juce::String& keys = {},
                     bool enabled = true, bool ticked = false)
    {
        juce::PopupMenu::Item item (text);
        item.itemID = id;
        item.shortcutKeyDescription = keys;
        item.isEnabled = enabled;
        item.isTicked = ticked;
        m.addItem (std::move (item));
    };

    const bool hasSession = session != nullptr;
    const bool hasClip = hasSession && session->getSelectedClip() != nullptr;

    switch (menuIndex)
    {
        case 0: // File
            add (cmdStartScreen, "Start Screen");
            m.addSeparator();
            add (cmdNew, "New Project...", "Ctrl+N");
            add (cmdOpen, "Open Project...", "Ctrl+O");
            add (cmdSave, "Save", "Ctrl+S", hasSession);
            add (cmdBackupNow, "Save Backup Now", {}, hasSession);
            add (cmdRestoreBackup, "Restore Backup...", {}, hasSession);
            m.addSeparator();
            add (cmdImport, "Import Audio...", {}, hasSession);
            add (cmdExport, "Export...", {}, hasSession);
            m.addSeparator();
            add (cmdSettings, "Settings...", "Ctrl+,");
            add (cmdAudioSettings, "Audio Settings...");
            add (cmdScanPlugins, "Scan Plugins...");
            add (cmdLatency, "Latency Check...", {}, hasSession);
            m.addSeparator();
            add (cmdExit, "Exit");
            break;

        case 1: // Edit
            add (cmdUndo, "Undo", "Ctrl+Z", hasSession);
            add (cmdRedo, "Redo", "Ctrl+Y", hasSession);
            m.addSeparator();
            add (cmdCopy, "Copy Clip", "Ctrl+C", hasClip);
            add (cmdPaste, "Paste Clip", "Ctrl+V", hasSession);
            add (cmdDuplicate, "Duplicate Clip", "Ctrl+D", hasClip);
            m.addSeparator();
            add (cmdSplit, "Split at Playhead", "S", hasClip);
            add (cmdDelete, "Delete Clip", "Del", hasClip);
            add (cmdSelectAll, "Select All Clips", "Ctrl+A", hasSession);
            break;

        case 2: // Track
            add (cmdAddAudioTrack, "Add Audio Track", {}, hasSession);
            add (cmdAddMidiTrack, "Add MIDI Track", {}, hasSession);
            break;

        case 3: // Transport
            add (cmdPlayStop, "Play / Stop", "Space", hasSession);
            add (cmdRecord, "Record", {}, hasSession);
            add (cmdRetake, "Re-take", "Ctrl+R", hasSession);
            add (cmdGoToStart, "Go to Start", "Home", hasSession);
            add (cmdAddMarker, "Add Marker at Playhead", {}, hasSession);
            add (cmdPrevMarker, "Previous Marker", "[", hasSession);
            add (cmdNextMarker, "Next Marker", "]", hasSession);
            m.addSeparator();
            add (cmdLoop, "Loop", {}, hasSession, hasSession && session->isLooping());
            add (cmdPunch, "Punch In/Out", {}, hasSession, hasSession && session->isPunchEnabled());
            add (cmdClick, "Click", {}, hasSession, hasSession && session->isClickEnabled());
            add (cmdSnap, "Snap", {}, hasSession, hasSession && session->isSnapEnabled());
            add (cmdMonitor, "Monitor", {}, hasSession, hasSession && session->isMonitoring());
            add (cmdAutoWrite, "Auto Write", {}, hasSession, hasSession && session->isAutomationWrite());
            break;

        case 4: // View
            add (cmdMixer, "Mixer", "M", hasSession, dockVisible && dock.getCurrentTab() == DockPanel::mixerTab);
            add (cmdDock, "Dock", "D", true, dockVisible);
            add (cmdBrowser, "Browser", "B", true, browserVisible);
            m.addSeparator();
            add (cmdZoomIn, "Zoom In", "Ctrl+=", hasSession);
            add (cmdZoomOut, "Zoom Out", "Ctrl+-", hasSession);
            add (cmdZoomFit, "Zoom to Fit", {}, hasSession);
            break;

        case 5: // Help
            add (cmdShortcuts, "Keyboard Shortcuts");
            break;

        default:
            break;
    }

    return m;
}

void MainComponent::menuItemSelected (int id, int)
{
    // These work with or without a project open
    switch (id)
    {
        case cmdStartScreen:   showStartScreen();   return;
        case cmdSettings:      showSettings();      return;
        case cmdNew:           newProject();        return;
        case cmdOpen:          openProject();       return;
        case cmdAudioSettings: showAudioSettings(); return;
        case cmdScanPlugins:   showPluginScanner(); return;
        case cmdShortcuts:     showShortcuts();     return;
        case cmdBrowser:       browserVisible = ! browserVisible; resized(); return;
        case cmdDock:          dockVisible = ! dockVisible; resized(); return;
        case cmdExit:          juce::JUCEApplication::getInstance()->systemRequestedQuit(); return;
        default: break;
    }

    if (session == nullptr)
        return;

    switch (id)
    {
        case cmdSave:          saveProject(); break;
        case cmdBackupNow:     session->makeBackup(); break;
        case cmdRestoreBackup: restoreBackup(); break;
        case cmdImport:        if (transportBar != nullptr) transportBar->importBeat(); break;
        case cmdExport:        if (transportBar != nullptr) transportBar->showExportDialog(); break;
        case cmdLatency:       showLatencyDialog(); break;

        case cmdUndo:          session->undo(); break;
        case cmdRedo:          session->redo(); break;
        case cmdCopy:          session->copySelectedClip(); break;
        case cmdPaste:         session->pasteClip(); break;
        case cmdDuplicate:     session->duplicateSelectedClip(); break;
        case cmdSplit:         session->splitSelectedClipAtPlayhead(); break;
        case cmdDelete:        session->deleteSelectedClip(); break;
        case cmdSelectAll:     session->selectAllClips(); break;

        case cmdAddAudioTrack: session->addTrack(); break;
        case cmdAddMidiTrack:  session->addMidiTrack(); break;

        case cmdPlayStop:      togglePlayStop(); break;
        case cmdRecord:        session->record(); break;
        case cmdRetake:        session->retake(); break;
        case cmdGoToStart:     session->getTransport().setPosition (tracktion::TimePosition()); break;
        case cmdAddMarker:     session->addMarkerAt (session->getTransport().getPosition().inSeconds()); break;
        case cmdPrevMarker:    session->jumpToMarker (-1); break;
        case cmdNextMarker:    session->jumpToMarker (1); break;
        case cmdLoop:          session->setLooping (! session->isLooping()); break;
        case cmdPunch:         session->setPunchEnabled (! session->isPunchEnabled()); break;
        case cmdClick:         session->setClickEnabled (! session->isClickEnabled()); break;
        case cmdSnap:          session->setSnapEnabled (! session->isSnapEnabled()); break;
        case cmdMonitor:       session->setMonitoring (! session->isMonitoring()); break;
        case cmdAutoWrite:     session->setAutomationWrite (! session->isAutomationWrite()); break;

        case cmdMixer:         toggleMixer(); break;
        case cmdZoomIn:        session->zoomAround (session->getViewStart() + session->getViewLength() * 0.5, 0.8); break;
        case cmdZoomOut:       session->zoomAround (session->getViewStart() + session->getViewLength() * 0.5, 1.25); break;
        case cmdZoomFit:       zoomToFit(); break;

        default: break;
    }
}

//==============================================================================
void MainComponent::togglePlayStop()
{
    if (session == nullptr)
        return;

    if (session->getTransport().isPlaying())
        session->stop();
    else
        session->play();
}

void MainComponent::zoomToFit()
{
    if (session == nullptr)
        return;

    const double length = juce::jmax (10.0, session->getEdit().getLength().inSeconds());
    session->setView (0.0, length * 1.05);
}

void MainComponent::showShortcuts()
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Keyboard Shortcuts",
        "Space\tPlay / Stop\n"
        "Home\tGo to start\n"
        "M\tMixer\n"
        "S\tSplit selected clip at playhead\n"
        "Del\tDelete selected clip\n"
        "Ctrl+Z / Ctrl+Y\tUndo / Redo\n"
        "Ctrl+C / Ctrl+V\tCopy / Paste clip\n"
        "Ctrl+D\tDuplicate clip\n"
        "Ctrl+R\tRe-take (while recording)\n"
        "Ctrl+N / O / S\tNew / Open / Save\n"
        "Ctrl+Scroll\tZoom\n"
        "Shift+Scroll\tScroll sideways\n"
        "Ctrl+= / Ctrl+-\tZoom in / out\n"
        "Alt+Drag\tMove without snapping\n\n"
        "Piano roll:\n"
        "Q\tQuantize\n"
        "Ctrl+A\tSelect all notes");
}

//==============================================================================
void MainComponent::showFxChain (tracktion::PluginList& list, const juce::String& title)
{
    if (session == nullptr)
        return;

    dock.showFx (list, title);
    openDock();
}

void MainComponent::showPianoRoll (tracktion::MidiClip& clip)
{
    if (session == nullptr)
        return;

    dock.showPianoRoll (clip);
    openDock();
}

void MainComponent::showComp (tracktion::WaveAudioClip& clip)
{
    if (session == nullptr)
        return;

    dock.showComp (clip);
    openDock();
}

void MainComponent::toggleMixer()
{
    if (session == nullptr)
        return;

    // M again while the mixer is showing hides the dock
    if (dockVisible && dock.getCurrentTab() == DockPanel::mixerTab)
    {
        dockVisible = false;
        resized();
        return;
    }

    dock.showTab (DockPanel::mixerTab);
    openDock();
}

void MainComponent::showPluginScanner()
{
    auto& pm = engine.getPluginManager();

    auto* comp = new juce::PluginListComponent (pm.pluginFormatManager,
                                                pm.knownPluginList,
                                                getProjectsRoot().getChildFile ("plugin_scan_crash.txt"),
                                                nullptr,
                                                false);
    comp->setSize (700, 500);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (comp);
    options.dialogTitle = "Plugins";
    options.dialogBackgroundColour = juce::Colour (0xff2b2f33);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

void MainComponent::showLatencyDialog()
{
    if (session == nullptr)
        return;

    auto* dialog = new LatencyDialog (*session, engine.getDeviceManager().deviceManager);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (dialog);
    options.dialogTitle = "Latency Check";
    options.dialogBackgroundColour = juce::Colour (0xff2b2f33);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (
        engine.getDeviceManager().deviceManager,
        0, 256,   // input channels min/max
        0, 256,   // output channels min/max
        true,     // show MIDI inputs
        false,    // show MIDI output selector
        false,    // show channels as stereo pairs
        false);   // hide advanced options behind a button

    selector->setSize (500, 450);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector);
    options.dialogTitle = "Audio Settings";
    options.dialogBackgroundColour = juce::Colour (0xff2b2f33);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));

    // Status bar
    g.setColour (juce::Colour (0xff17191b));
    g.fillRect (getLocalBounds().removeFromBottom (statusHeight));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    menuBar.setBounds (area.removeFromTop (24));

    auto status = area.removeFromBottom (statusHeight);
    projectLabel.setBounds (status.removeFromLeft (320).reduced (8, 0));
    deviceLabel.setBounds (status.reduced (8, 0));

    area = area.reduced (10, 8);

    if (transportBar != nullptr)
        transportBar->setBounds (area.removeFromTop (40));

    area.removeFromTop (8);

    // Dock along the bottom
    dock.setVisible (dockVisible);
    dockDivider.setVisible (dockVisible);

    if (dockVisible)
    {
        dock.setBounds (area.removeFromBottom (juce::jmin (dockHeight, juce::jmax (150, area.getHeight() - 200))));
        dockDivider.setBounds (area.removeFromBottom (8));
    }

    // Browser down the left side: Sounds and Plugins tabs
    browserTabs.setVisible (browserVisible);

    if (browserVisible)
    {
        browserTabs.setBounds (area.removeFromLeft (240));
        area.removeFromLeft (8);
    }

    // Master strip along the bottom
    if (masterStrip != nullptr)
        masterStrip->setBounds (area.removeFromBottom (40));

    area.removeFromBottom (6);

    // Ruler lines up with the track lanes (TracksView headerWidth 240 + gap 4)
    constexpr int laneOffset = 244;
    auto markerRow = area.removeFromTop (18);

    if (markerBar != nullptr)
        markerBar->setBounds (markerRow.withTrimmedLeft (laneOffset)
                                       .withTrimmedRight (tracksViewport.getScrollBarThickness()));

    auto patternRow = area.removeFromTop (24);

    if (patternLane != nullptr)
        patternLane->setBounds (patternRow.withTrimmedLeft (laneOffset)
                                          .withTrimmedRight (tracksViewport.getScrollBarThickness()));

    auto rulerRow = area.removeFromTop (24);

    if (ruler != nullptr)
        ruler->setBounds (rulerRow.withTrimmedLeft (laneOffset)
                                  .withTrimmedRight (tracksViewport.getScrollBarThickness()));

    area.removeFromTop (4);
    tracksViewport.setBounds (area);

    // At least as tall as the view, so the empty space below the tracks takes drops
    if (tracksView != nullptr)
        tracksView->setSize (tracksViewport.getWidth() - tracksViewport.getScrollBarThickness(),
                             juce::jmax (tracksView->getIdealHeight(), tracksViewport.getHeight()));

    startScreen.setBounds (getLocalBounds());
}

void MainComponent::mouseDown (const juce::MouseEvent&)
{
    grabKeyboardFocus();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    // Keys don't reach the project while the start screen is up
    if (startScreenVisible)
        return false;

    if (session == nullptr)
        return false;

    const auto mods = key.getModifiers();
    const int code  = key.getKeyCode();

    // Left / Right move the playhead a beat (Ctrl: a bar). Hold to skim.
    if (session != nullptr && (key.isKeyCode (juce::KeyPress::leftKey) || key.isKeyCode (juce::KeyPress::rightKey)))
    {
        auto& tempo = session->getEdit().tempoSequence;
        auto& transport = session->getTransport();

        const double step = key.getModifiers().isCommandDown() ? 4.0 : 1.0;
        const double beat = tempo.toBeats (transport.getPosition()).inBeats();
        const bool forward = key.isKeyCode (juce::KeyPress::rightKey);

        const double target = forward ? (std::floor (beat / step + 1e-6) + 1.0) * step
                                      : (std::ceil (beat / step - 1e-6) - 1.0) * step;

        transport.setPosition (tempo.toTime (tracktion::BeatPosition::fromBeats (juce::jmax (0.0, target))));
        return true;
    }

    if (mods.isCommandDown())
    {
        // Ctrl+Z undo, Ctrl+Shift+Z redo
        if (code == 'Z' || code == 'z')
        {
            if (mods.isShiftDown())
                session->redo();
            else
                session->undo();

            return true;
        }

        if (code == 'Y' || code == 'y')  { session->redo();                  return true; }
        if (code == 'S' || code == 's')  { saveProject();                    return true; }
        if (code == 'O' || code == 'o')  { openProject();                    return true; }
        if (code == 'N' || code == 'n')  { newProject();                     return true; }
        if (code == 'D' || code == 'd')  { session->duplicateSelectedClip(); return true; }
        if (code == 'C' || code == 'c')  { session->copySelectedClip();      return true; }
        if (code == 'V' || code == 'v')  { session->pasteClip();             return true; }
        if (code == 'A' || code == 'a')  { session->selectAllClips();        return true; }
        if (code == 'R' || code == 'r')  { session->retake();                return true; }
        if (code == ',')                 { showSettings();                   return true; }

        if (code == '=' || code == '+')
        {
            session->zoomAround (session->getViewStart() + session->getViewLength() * 0.5, 0.8);
            return true;
        }

        if (code == '-')
        {
            session->zoomAround (session->getViewStart() + session->getViewLength() * 0.5, 1.25);
            return true;
        }

        return false;
    }

    // D shows and hides the dock
    if (code == 'D' || code == 'd')
    {
        dockVisible = ! dockVisible;
        resized();
        return true;
    }

    // B shows and hides the browser
    if (code == 'B' || code == 'b')
    {
        browserVisible = ! browserVisible;
        resized();
        return true;
    }

    // M opens and closes the mixer
    if (code == 'M' || code == 'm')
    {
        toggleMixer();
        return true;
    }

    // [ and ] jump between markers
    if (code == '[')
    {
        session->jumpToMarker (-1);
        return true;
    }

    if (code == ']')
    {
        session->jumpToMarker (1);
        return true;
    }

    if (code == juce::KeyPress::homeKey)
    {
        session->getTransport().setPosition (tracktion::TimePosition());
        return true;
    }

    // Space toggles play/stop
    if (code == juce::KeyPress::spaceKey)
    {
        togglePlayStop();
        return true;
    }

    return false;
}

//==============================================================================
void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const auto before = deviceLabel.getText();
    updateDeviceLabel();

    // Only rewire inputs when the device actually changed
    if (session != nullptr && deviceLabel.getText() != before)
        session->refreshInputs();
}

void MainComponent::updateDeviceLabel()
{
    auto& dm = engine.getDeviceManager().deviceManager;

    if (auto* device = dm.getCurrentAudioDevice())
    {
        deviceLabel.setText (device->getTypeName()
                                 + "  |  " + device->getName()
                                 + "  |  " + juce::String (device->getCurrentSampleRate(), 0) + " Hz"
                                 + "  |  " + juce::String (device->getCurrentBufferSizeSamples()) + " samples"
                                 + "  |  " + juce::String (device->getActiveInputChannels().countNumberOfSetBits()) + " inputs",
                             juce::dontSendNotification);
    }
    else
    {
        deviceLabel.setText ("No audio device", juce::dontSendNotification);
    }
}

void MainComponent::showClipProps (tracktion::AudioClipBase& clip)
{
    if (session == nullptr)
        return;

    auto* comp = new ClipPropsComponent (*session, clip);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (comp);
    options.dialogTitle = "Stretch, Pitch & Gain";
    options.dialogBackgroundColour = juce::Colour (0xff2b2f33);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void MainComponent::restoreBackup()
{
    if (session == nullptr || isBusyRecording())
        return;

    chooser = std::make_unique<juce::FileChooser> ("Restore Backup", session->getBackupsFolder(), "*.tracktionedit");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto backup = fc.getResult();

                              if (! backup.existsAsFile() || session == nullptr)
                                  return;

                              // Back up what you have now, so a restore can always be undone
                              session->makeBackup();

                              const auto projectFile = session->getProjectFile();
                              closeProject();

                              backup.copyFileTo (projectFile);
                              loadProject (projectFile);
                          });
}

void MainComponent::addDrumTrack()
{
    if (session == nullptr)
        return;

    session->addDrumTrack();
    showDrumRack (session->getNumTracks() - 1);
}

void MainComponent::showDrumRack (int trackIndex)
{
    if (session == nullptr)
        return;

    if (auto* track = session->getTrack (trackIndex))
    {
        dock.showInstrument (*track, true);
        openDock();
    }
}

void MainComponent::showSampler (int trackIndex)
{
    if (session == nullptr)
        return;

    if (auto* track = session->getTrack (trackIndex))
    {
        dock.showInstrument (*track, false);
        openDock();
    }
}

void MainComponent::openDock()
{
    if (! dockVisible)
    {
        dockVisible = true;
        resized();
    }
}

void MainComponent::showStartScreen()
{
    startScreenVisible = true;
    startScreen.refresh (session != nullptr ? session->getProjectFile() : juce::File());
    startScreen.setVisible (true);
    startScreen.toFront (true);
    startScreen.grabKeyboardFocus();
    resized();
}

void MainComponent::hideStartScreen()
{
    startScreenVisible = false;
    startScreen.setVisible (false);
    grabKeyboardFocus();
}

void MainComponent::showSettings()
{
    auto* settings = new SettingsComponent (engine);
    settings->onLatencyCheck = [this] { showLatencyDialog(); };

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (settings);
    options.dialogTitle = "Settings";
    options.dialogBackgroundColour = juce::Colour (0xff1e2124);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

void MainComponent::setUpNewSong()
{
    if (session == nullptr)
        return;

    if (pendingBpm > 0.0)
        session->setBpm (pendingBpm);

    // A vocal track on top, ready to record, with your saved vocal chain if you have one
    if (auto* vocal = session->getTrack (0))
        vocal->setName ("Vocal");

    if (session->getTemplateNames().contains ("Vocal Chain"))
        session->loadChainTemplate (0, "Vocal Chain");

    session->setMonitoring (true);
    session->setCountInBars (1);

    if (pendingBeatFile.existsAsFile())
    {
        // Started from a beat file: put it on its own track at bar 1, and keep the dock out of the way
        session->addTrack();
        const int beatTrack = session->getNumTracks() - 1;
        session->importAudioFileAt (pendingBeatFile, beatTrack, 0.0);

        if (auto* beat = session->getTrack (beatTrack))
            beat->setName ("Beat");

        dockVisible = false;
    }
    else
    {
        // Fresh song: start in the Rack
        dockVisible = true;
        dock.showTab (DockPanel::rackTab);
    }

    session->save();
    resized();
}
