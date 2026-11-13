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
            m.addSeparator();
            add (cmdMidiLearn, "MIDI Learn", "Ctrl+L", hasSession, hasSession && session->isMidiLearn());
            add (cmdMidiMappings, "MIDI Mappings...", {}, hasSession);
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
        case cmdMidiLearn:     session->setMidiLearn (! session->isMidiLearn()); break;
        case cmdMidiMappings:  showMidiMappings(); break;

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
