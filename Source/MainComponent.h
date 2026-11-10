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
#include "PatternLane.h"
#include "ClipProps.h"
#include "KitBrowser.h"
#include "DrumRack.h"
#include "SamplerView.h"
#include "DockPanel.h"
#include "StartScreen.h"
#include "SettingsComponent.h"

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

    // Menu bar
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

private:
    enum CommandIDs
    {
        cmdNew = 1, cmdOpen, cmdSave, cmdImport, cmdExport, cmdSettings, cmdAudioSettings, cmdScanPlugins, cmdLatency, cmdExit,
        cmdBackupNow, cmdRestoreBackup, cmdStartScreen,
        cmdUndo = 100, cmdRedo, cmdCopy, cmdPaste, cmdDuplicate, cmdSplit, cmdDelete, cmdSelectAll,
        cmdAddAudioTrack = 200, cmdAddMidiTrack, cmdAddDrumTrack,
        cmdPlayStop = 300, cmdRecord, cmdGoToStart, cmdLoop, cmdClick, cmdSnap, cmdMonitor, cmdAutoWrite, cmdPunch,
        cmdAddMarker, cmdPrevMarker, cmdNextMarker,
        cmdMixer = 400, cmdZoomIn, cmdZoomOut, cmdZoomFit, cmdBrowser, cmdDock,
        cmdShortcuts = 500
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void showAudioSettings();
    void showSettings();
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
    void addDrumTrack();
    void showShortcuts();
    void toggleMixer();
    void togglePlayStop();
    void zoomToFit();
    void updateDeviceLabel();

    // Projects
    void loadProject (const juce::File& editFile);
    void closeProject();
    void newProject();
    void openProject();
    void saveProject();
    void createProject (const juce::String& name);
    bool isBusyRecording() const;

    static juce::File getProjectsRoot();
    static juce::File getLastProjectFile();
    static void setLastProjectFile (const juce::File& editFile);

    tracktion::Engine& engine;

    juce::MenuBarComponent menuBar;
    juce::Label projectLabel, deviceLabel;

    std::unique_ptr<EditSession> session;
    std::unique_ptr<TransportBar> transportBar;
    std::unique_ptr<TimelineRuler> ruler;
    std::unique_ptr<MarkerBar> markerBar;
    std::unique_ptr<PatternLane> patternLane;
    std::unique_ptr<MasterStrip> masterStrip;

    juce::Viewport tracksViewport;
    std::unique_ptr<TracksView> tracksView;

    juce::OwnedArray<FxChainWindow> fxWindows;
    juce::OwnedArray<PianoRollWindow> pianoRollWindows;
    std::unique_ptr<MixerWindow> mixerWindow;
    juce::OwnedArray<CompWindow> compWindows;
    juce::OwnedArray<DrumRackWindow> drumRackWindows;
    juce::OwnedArray<SamplerWindow> samplerWindows;
    std::unique_ptr<KitBrowser> kitBrowser;
    bool browserVisible = true;

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
