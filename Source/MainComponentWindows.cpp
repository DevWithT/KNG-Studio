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
    songWindows.add (options.launchAsync());
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
    songWindows.add (options.launchAsync());
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

void MainComponent::showMidiMappings()
{
    if (session == nullptr)
        return;

    // Already open: bring it forward instead of opening a second one
    if (midiMapWindow != nullptr)
    {
        midiMapWindow->toFront (true);
        return;
    }

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new MidiMapDialog (*session));
    options.dialogTitle = "MIDI Mappings";
    options.dialogBackgroundColour = juce::Colour (0xff2b2f33);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    midiMapWindow = options.launchAsync();
}

void MainComponent::toggleRack()
{
    if (session == nullptr)
        return;

    // R again while the Rack is showing hides the bottom panel
    if (dockVisible && dock.getCurrentTab() == DockPanel::rackTab)
    {
        dockVisible = false;
        resized();
        return;
    }

    dock.showTab (DockPanel::rackTab);
    openDock();
}
