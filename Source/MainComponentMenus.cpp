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
#include "UndoHistory.h"
#include "RoutingMatrix.h"
#include "PluginCpuMonitor.h"
#include "MasterScope.h"

// Command id for Undo History. It sits outside the MainComponent command list on purpose.
static constexpr int cmdUndoHistory = 9001;
static constexpr int cmdLayoutLoad = 9100;   // 9101 to 9103
static constexpr int cmdLayoutSave = 9200;   // 9201 to 9203

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
            add (cmdSaveTemplate, "Save as Template...", {}, hasSession);
            m.addSeparator();
            add (cmdImport, "Import Audio...", {}, hasSession);
            add (9005, "Collect and Save...", {}, hasSession);
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
            add (cmdUndoHistory, "Undo History...", {}, hasSession);
            m.addSeparator();
            add (9006, "Cut Clip", "Ctrl+X", hasClip);
            add (cmdCopy, "Copy Clip", "Ctrl+C", hasClip);
            add (cmdPaste, "Paste Clip", "Ctrl+V", hasSession);
            add (cmdDuplicate, "Duplicate Clip", "Ctrl+D", hasClip);
            m.addSeparator();
            add (cmdSplit, "Split at Playhead", "S", hasClip);
            add (cmdDelete, "Delete Clip", "Del", hasClip);
            add (cmdSelectAll, "Select All Clips", "Ctrl+A", hasSession);
            add (9007, "Select None", "Ctrl+Shift+A", hasSession);
            add (9008, "Invert Selection", "Ctrl+I", hasSession);
            add (9009, "Swap Clips", {}, hasSession && session->getSelectedClips().size() == 2);
            m.addSeparator();
            add (cmdMidiLearn, "MIDI Learn", "Ctrl+L", hasSession, hasSession && session->isMidiLearn());
            add (cmdMidiMappings, "MIDI Mappings...", {}, hasSession);
            break;

        case 2: // Track
            add (cmdAddAudioTrack, "Add Audio Track", {}, hasSession);
            add (cmdAddMidiTrack, "Add MIDI Track", {}, hasSession);
            add (9010, "Duplicate Lane", {}, hasSession && session->isLaneTrack (session->getSelectedTrackIndex()));
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
            add (cmdAutoWrite, "Auto Write", {}, hasSession, hasSession && session->isAutomationWrite());
            break;

        case 4: // View
            add (cmdMixer, "Mixer", "M", hasSession, dockVisible && dock.getCurrentTab() == DockPanel::mixerTab);
            add (cmdDock, "Dock", "D", true, dockVisible);
            add (cmdBrowser, "Browser", "B", true, browserVisible);
            add (cmdPianoKeys, "Computer Keyboard Piano", "Ctrl+K", hasSession, pianoKeysOn);
            add (9002, "Routing Matrix...", {}, hasSession);
            add (9003, "Plugin CPU Monitor...", {}, hasSession);
            add (9004, "Master Output...", {}, hasSession);

            {
                juce::PopupMenu layouts;

                for (int slot = 1; slot <= 3; ++slot)
                    layouts.addItem (cmdLayoutLoad + slot, "Layout " + juce::String (slot));

                layouts.addSeparator();

                for (int slot = 1; slot <= 3; ++slot)
                    layouts.addItem (cmdLayoutSave + slot, "Save as Layout " + juce::String (slot));

                m.addSubMenu ("Layouts", layouts);
            }
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
    // Window layouts
    if (id > cmdLayoutLoad && id <= cmdLayoutLoad + 3)
    {
        loadLayout (id - cmdLayoutLoad);
        return;
    }

    if (id > cmdLayoutSave && id <= cmdLayoutSave + 3)
    {
        saveLayout (id - cmdLayoutSave);
        return;
    }

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
        case cmdSaveTemplate:  saveAsTemplate(); break;
        case cmdImport:        if (transportBar != nullptr) transportBar->importBeat(); break;
        case cmdExport:        if (transportBar != nullptr) transportBar->showExportDialog(); break;
        case cmdLatency:       showLatencyDialog(); break;

        case cmdUndo:          session->undo(); break;
        case cmdRedo:          session->redo(); break;
        case 9004: if (auto* w = MasterScope::open (*session)) songWindows.add (w); break;
        case 9005:
        {
            int missing = 0;
            const int copied = session->collectAndSave (&missing);

            juce::String msg;

            if (copied < 0)
                msg = "Stop recording first.";
            else if (copied == 0 && missing == 0)
                msg = "Everything is already inside the project folder.";
            else
                msg = "Copied " + juce::String (copied) + (copied == 1 ? " file" : " files") + " into the project's Audio folder.";

            if (missing > 0)
                msg += "\n\n" + juce::String (missing) + (missing == 1 ? " file was" : " files were") + " missing or could not be copied, so they were left as they are.";

            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Collect and Save", msg);
            break;
        }
        case 9003: if (auto* w = PluginCpuMonitor::open (*session)) songWindows.add (w); break;
        case 9002: if (auto* w = RoutingMatrix::open (*session)) songWindows.add (w); break;
        case cmdUndoHistory:   if (auto* w = UndoHistoryList::open (*session)) songWindows.add (w); break;
        case cmdCopy:          session->copySelectedClip(); break;
        case cmdPaste:         session->pasteClip(); break;
        case cmdDuplicate:     session->duplicateSelectedClip(); break;
        case cmdSplit:         session->splitSelectedClipAtPlayhead(); break;
        case cmdDelete:        session->deleteSelectedClip(); break;
        case cmdSelectAll:     session->selectAllClips(); break;
        case 9006:             session->cutSelectedClip(); break;
        case 9007:             session->clearSelection(); break;
        case 9008:             session->invertSelection(); break;
        case 9009:             session->swapSelectedClips(); break;
        case 9010:
        {
            const int n = session->duplicateLane (session->getSelectedTrackIndex());

            if (n >= 0)
                session->selectTrack (n);

            break;
        }
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
        case cmdAutoWrite:     session->setAutomationWrite (! session->isAutomationWrite()); break;

        case cmdMixer:         toggleMixer(); break;
        case cmdZoomIn:        session->zoomAround (session->getViewStart() + session->getViewLength() * 0.5, 0.8); break;
        case cmdZoomOut:       session->zoomAround (session->getViewStart() + session->getViewLength() * 0.5, 1.25); break;
        case cmdZoomFit:       zoomToFit(); break;
        case cmdPianoKeys:     setPianoKeys (! pianoKeysOn); break;

        default: break;
    }
}
