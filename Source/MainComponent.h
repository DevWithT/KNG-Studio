#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <tracktion_engine/tracktion_engine.h>
#include "EditSession.h"
#include "TransportBar.h"
#include "TracksView.h"
#include "TimelineRuler.h"
#include "MasterStrip.h"
#include "FxChainComponent.h"
#include "LatencyDialog.h"
#include "PianoRoll.h"
#include "MixerView.h"
#include "CompView.h"
#include "MarkerBar.h"
#include "ClipProps.h"
#include "KitBrowser.h"
#include "DrumRack.h"
#include "DockPanel.h"
#include "StartScreen.h"
#include "SettingsComponent.h"
#include "PluginBrowser.h"
#include "TrackInspector.h"
#include "PatternListPanel.h"
#include "PlaylistToolbar.h"
#include "HintBar.h"
#include "PlaylistScrollBar.h"

class MainComponent : public juce::Component,
                      public juce::MenuBarModel,
                      public juce::DragAndDropContainer,
                      private juce::ChangeListener
{
public:
    explicit MainComponent (tracktion::Engine& engineToUse);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;

    // Menu bar
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

private:
    enum CommandIDs
    {
        cmdNew = 1, cmdOpen, cmdSave, cmdImport, cmdExport, cmdSettings, cmdAudioSettings, cmdScanPlugins, cmdLatency, cmdExit,
        cmdBackupNow, cmdRestoreBackup, cmdStartScreen, cmdSaveTemplate,
        cmdUndo = 100, cmdRedo, cmdCopy, cmdPaste, cmdDuplicate, cmdSplit, cmdDelete, cmdSelectAll,
        cmdAddAudioTrack = 200, cmdAddMidiTrack,
        cmdPlayStop = 300, cmdRecord, cmdRetake, cmdMidiLearn, cmdMidiMappings, cmdGoToStart, cmdLoop, cmdClick, cmdSnap, cmdMonitor, cmdAutoWrite, cmdPunch,
        cmdAddMarker, cmdPrevMarker, cmdNextMarker,
        cmdMixer = 400, cmdZoomIn, cmdZoomOut, cmdZoomFit, cmdBrowser, cmdDock, cmdPianoKeys,
        cmdShortcuts = 500
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void showAudioSettings();
    void showSettings();
    void showMidiMappings();
    void saveLayout (int slot);
    void loadLayout (int slot);
    juce::Component::SafePointer<juce::DialogWindow> midiMapWindow;
    juce::Array<juce::Component::SafePointer<juce::DialogWindow>> songWindows;
    void showPluginScanner();
    void showLatencyDialog();
    void showFxChain (tracktion::PluginList& list, const juce::String& title);
    void showPianoRoll (tracktion::MidiClip& clip);
    void showComp (tracktion::WaveAudioClip& clip);
    void showClipProps (tracktion::AudioClipBase& clip);
    void restoreBackup();
    void showDrumRack (int trackIndex);
    void showSampler (int trackIndex);
    void openDock();
    void showStartScreen();
    void hideStartScreen();
    void setUpNewSong();
    void showShortcuts();
    void toggleMixer();
    void toggleRack();
    void togglePlayStop();
    void zoomToFit();
    void updateDeviceLabel();

    // Computer keyboard piano
    void setPianoKeys (bool on);
    void releasePianoKeys();
    bool handlePianoKey (const juce::KeyPress&);
    int pianoNoteFor (const juce::KeyPress&) const;
    bool pianoKeysOn = false;
    int pianoBase = 48;                   // note of the lowest key (C3)
    std::map<int, int> heldPianoKeys;     // key code -> note being held

    // Projects
    void loadProject (const juce::File& editFile);
    void closeProject();
    void newProject();
    void openProject();
    void saveProject();
    void createProject (const juce::String& name);
    void saveAsTemplate();
    void newFromTemplate (const juce::String& templateName);
    bool isBusyRecording() const;

    static juce::File getProjectsRoot();
    static juce::File getLastProjectFile();
    static void setLastProjectFile (const juce::File& editFile);

    tracktion::Engine& engine;

    juce::MenuBarComponent menuBar;
    juce::Label projectLabel, deviceLabel;
    HintBar hintBar;

    std::unique_ptr<EditSession> session;
    std::unique_ptr<TransportBar> transportBar;
    std::unique_ptr<TrackInspector> inspector;
    bool inspectorVisible = true;
    std::unique_ptr<PatternListPanel> patternList;
    bool patternsVisible = true;
    std::unique_ptr<PlaylistToolbar> playlistToolbar;
    std::unique_ptr<PlaylistScrollBar> playlistScroll;
    std::unique_ptr<TimelineRuler> ruler;
    std::unique_ptr<MarkerBar> markerBar;
    std::unique_ptr<MasterStrip> masterStrip;

    juce::Viewport tracksViewport;
    std::unique_ptr<TracksView> tracksView;

    std::unique_ptr<KitBrowser> kitBrowser;
    bool browserVisible = true;

    // Left column: Sounds (samples) and Plugins tabs
    std::unique_ptr<PluginBrowser> pluginBrowser;
    juce::TabbedComponent browserTabs { juce::TabbedButtonBar::TabsAtTop };

    DockPanel dock;
    juce::TooltipWindow tooltipWindow { this, 500 };
    StartScreen startScreen;
    bool startScreenVisible = true;
    int pendingStartMode = 0;   // 1 = new beat, 2 = new vocal session
    double pendingBpm = 0.0;
    juce::File pendingBeatFile;
    DockDivider dockDivider;
    int dockHeight = 340;
    int dockDragStartHeight = 340;
    bool dockVisible = true;
    std::unique_ptr<juce::FileChooser> chooser;

    static constexpr int statusHeight = 22;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
