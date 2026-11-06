#include "MainComponent.h"

MainComponent::MainComponent (tracktion::Engine& engineToUse)
    : engine (engineToUse)
{
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

    tracksViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (tracksViewport);

    engine.getDeviceManager().deviceManager.addChangeListener (this);
    updateDeviceLabel();

    loadProject (getLastProjectFile());

    setSize (1280, 800);
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
    fxWindows.clear();
    pianoRollWindows.clear();
    mixerWindow = nullptr;
    compWindows.clear();
    tracksViewport.setViewedComponent (nullptr, false);
    tracksView = nullptr;
    masterStrip = nullptr;
    ruler = nullptr;
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

    transportBar = std::make_unique<TransportBar> (*session);
    addAndMakeVisible (*transportBar);

    ruler = std::make_unique<TimelineRuler> (*session);
    addAndMakeVisible (*ruler);

    masterStrip = std::make_unique<MasterStrip> (*session);
    addAndMakeVisible (*masterStrip);

    tracksView = std::make_unique<TracksView> (*session);
    tracksViewport.setViewedComponent (tracksView.get(), false);

    projectLabel.setText ("Project: " + session->getProjectName(), juce::dontSendNotification);
    setLastProjectFile (editFile);

    resized();
}

void MainComponent::newProject()
{
    if (isBusyRecording())
        return;

    auto* w = new juce::AlertWindow ("New Project", "Name your project:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "My Song");
    w->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1)
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
            add (cmdNew, "New Project...", "Ctrl+N");
            add (cmdOpen, "Open Project...", "Ctrl+O");
            add (cmdSave, "Save", "Ctrl+S", hasSession);
            m.addSeparator();
            add (cmdImport, "Import Audio...", {}, hasSession);
            add (cmdExport, "Export...", {}, hasSession);
            m.addSeparator();
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
            break;

        case 2: // Track
            add (cmdAddAudioTrack, "Add Audio Track", {}, hasSession);
            add (cmdAddMidiTrack, "Add MIDI Track", {}, hasSession);
            break;

        case 3: // Transport
            add (cmdPlayStop, "Play / Stop", "Space", hasSession);
            add (cmdRecord, "Record", {}, hasSession);
            add (cmdGoToStart, "Go to Start", "Home", hasSession);
            m.addSeparator();
            add (cmdLoop, "Loop", {}, hasSession, hasSession && session->isLooping());
            add (cmdPunch, "Punch In/Out", {}, hasSession, hasSession && session->isPunchEnabled());
            add (cmdClick, "Click", {}, hasSession, hasSession && session->isClickEnabled());
            add (cmdSnap, "Snap", {}, hasSession, hasSession && session->isSnapEnabled());
            add (cmdMonitor, "Monitor", {}, hasSession, hasSession && session->isMonitoring());
            add (cmdAutoWrite, "Auto Write", {}, hasSession, hasSession && session->isAutomationWrite());
            break;

        case 4: // View
            add (cmdMixer, "Mixer", "M", hasSession, mixerWindow != nullptr && mixerWindow->isVisible());
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
        case cmdNew:           newProject();        return;
        case cmdOpen:          openProject();       return;
        case cmdAudioSettings: showAudioSettings(); return;
        case cmdScanPlugins:   showPluginScanner(); return;
        case cmdShortcuts:     showShortcuts();     return;
        case cmdExit:          juce::JUCEApplication::getInstance()->systemRequestedQuit(); return;
        default: break;
    }

    if (session == nullptr)
        return;

    switch (id)
    {
        case cmdSave:          saveProject(); break;
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

        case cmdAddAudioTrack: session->addTrack(); break;
        case cmdAddMidiTrack:  session->addMidiTrack(); break;

        case cmdPlayStop:      togglePlayStop(); break;
        case cmdRecord:        session->record(); break;
        case cmdGoToStart:     session->getTransport().setPosition (tracktion::TimePosition()); break;
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

    for (auto* w : fxWindows)
    {
        if (w->getList() == &list)
        {
            w->setVisible (true);
            w->toFront (true);
            return;
        }
    }

    fxWindows.add (new FxChainWindow (title, *session, list));
}

void MainComponent::showPianoRoll (tracktion::MidiClip& clip)
{
    if (session == nullptr)
        return;

    for (auto* w : pianoRollWindows)
    {
        if (w->getClip() == &clip)
        {
            w->setVisible (true);
            w->toFront (true);
            return;
        }
    }

    pianoRollWindows.add (new PianoRollWindow (*session, clip));
}

void MainComponent::showComp (tracktion::WaveAudioClip& clip)
{
    if (session == nullptr)
        return;

    for (auto* w : compWindows)
    {
        if (w->getClip() == &clip)
        {
            w->setVisible (true);
            w->toFront (true);
            return;
        }
    }

    compWindows.add (new CompWindow (*session, clip));
}

void MainComponent::toggleMixer()
{
    if (session == nullptr)
        return;

    if (mixerWindow == nullptr)
    {
        mixerWindow = std::make_unique<MixerWindow> (*session);
        return;
    }

    mixerWindow->setVisible (! mixerWindow->isVisible());

    if (mixerWindow->isVisible())
        mixerWindow->toFront (true);
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

    // Master strip along the bottom
    if (masterStrip != nullptr)
        masterStrip->setBounds (area.removeFromBottom (40));

    area.removeFromBottom (6);

    // Ruler lines up with the track lanes (TracksView headerWidth 240 + gap 4)
    constexpr int laneOffset = 244;
    auto rulerRow = area.removeFromTop (24);

    if (ruler != nullptr)
        ruler->setBounds (rulerRow.withTrimmedLeft (laneOffset)
                                  .withTrimmedRight (tracksViewport.getScrollBarThickness()));

    area.removeFromTop (4);
    tracksViewport.setBounds (area);

    if (tracksView != nullptr)
        tracksView->setSize (tracksViewport.getWidth() - tracksViewport.getScrollBarThickness(),
                             tracksView->getIdealHeight());
}

void MainComponent::mouseDown (const juce::MouseEvent&)
{
    grabKeyboardFocus();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (session == nullptr)
        return false;

    const auto mods = key.getModifiers();
    const int code  = key.getKeyCode();

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

    // M opens and closes the mixer
    if (code == 'M' || code == 'm')
    {
        toggleMixer();
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
