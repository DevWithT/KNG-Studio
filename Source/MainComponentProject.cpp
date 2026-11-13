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
#include "KngLoudnessPlugin.h"
#include "KngFattenerPlugin.h"
#include "Kng808Plugin.h"
#include "KngAirPlugin.h"
#include "KngGatePlugin.h"
#include "KngWidthPlugin.h"
#include "KngRadioPlugin.h"
#include "KngThrowPlugin.h"
#include "PluginWindow.h"
#include "MidiMapDialog.h"
#include "KngPopOut.h"

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
    // Stop learning and keep any controller links before the song closes
    if (session != nullptr)
        session->setMidiLearn (false);

    // The MIDI Mappings window reads the song, so it closes with it
    if (midiMapWindow != nullptr)
        delete midiMapWindow.getComponent();

    // Clip Properties and Latency Check also read the song
    for (auto& w : songWindows)
        if (w != nullptr)
            delete w.getComponent();

    songWindows.clear();

    // Pop-out plugin windows read the song too
    KngPopOut::closeAll();

    KngUIBehaviour::setCurrentEdit (nullptr);
    dock.setSession (nullptr);
    if (pluginBrowser != nullptr)
        pluginBrowser->setSession (nullptr);
    tracksViewport.setViewedComponent (nullptr, false);
    tracksView = nullptr;
    masterStrip = nullptr;
    ruler = nullptr;
    markerBar = nullptr;
    transportBar = nullptr;
    inspector = nullptr;
    patternList = nullptr;
    playlistToolbar = nullptr;
    session = nullptr;   // saves on the way out
}

void MainComponent::loadProject (const juce::File& editFile)
{
    closeProject();

    session = std::make_unique<EditSession> (engine, editFile);
    KngUIBehaviour::setCurrentEdit (&session->getEdit());
    session->getEdit().getParameterControlMappings().loadFromEdit();
    session->onShowFxChain   = [this] (tracktion::PluginList& list, const juce::String& title) { showFxChain (list, title); };
    session->onShowPianoRoll = [this] (tracktion::MidiClip& clip) { showPianoRoll (clip); };
    session->onShowComp      = [this] (tracktion::WaveAudioClip& clip) { showComp (clip); };
    session->onShowClipProps = [this] (tracktion::AudioClipBase& clip) { showClipProps (clip); };
    session->onShowDrumRack  = [this] (int trackIndex) { showDrumRack (trackIndex); };
    session->onShowSampler   = [this] (int trackIndex) { showSampler (trackIndex); };
    session->onShowPluginEditor = [this] (tracktion::Plugin& p) { dock.showEffect (p); openDock(); };
    KngPopOut::setDockHandler ([this] (tracktion::Plugin& p) { dock.showEffect (p); openDock(); });
    session->onShowRack = [this] { dock.showTab (DockPanel::rackTab); openDock(); };
    dock.setSession (session.get());
    pluginBrowser->setSession (session.get());

    transportBar = std::make_unique<TransportBar> (*session);
    addAndMakeVisible (*transportBar);
    transportBar->onShowRack  = [this] { toggleRack(); };
    transportBar->onShowMixer = [this] { toggleMixer(); };

    inspector = std::make_unique<TrackInspector> (*session);
    addAndMakeVisible (*inspector);

    patternList = std::make_unique<PatternListPanel> (*session);
    patternList->onPatternPicked = [this] { dock.showTab (DockPanel::rackTab); openDock(); };
    addAndMakeVisible (*patternList);

    playlistToolbar = std::make_unique<PlaylistToolbar> (*session);
    addAndMakeVisible (*playlistToolbar);

    ruler = std::make_unique<TimelineRuler> (*session);
    addAndMakeVisible (*ruler);

    markerBar = std::make_unique<MarkerBar> (*session);
    addAndMakeVisible (*markerBar);

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
