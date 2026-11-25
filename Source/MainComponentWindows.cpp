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
#include "KngTheme.h"

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
    options.dialogBackgroundColour = KngTheme::bg;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
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
    options.dialogBackgroundColour = KngTheme::bg;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
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
    options.dialogBackgroundColour = KngTheme::bg;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
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
    options.dialogBackgroundColour = KngTheme::bg;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
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
    options.dialogBackgroundColour = KngTheme::bg;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
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
    options.dialogBackgroundColour = KngTheme::bg;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = true;
    midiMapWindow = options.launchAsync();
}

//==============================================================================
// Window layouts: three slots, kept between launches in Documents\KNG Studio\layouts.xml.
// A layout remembers which panels are showing, the bottom panel's height and tab, and the left column's tab.

static juce::File getLayoutsFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("KNG Studio").getChildFile ("layouts.xml");
}

void MainComponent::saveLayout (int slot)
{
    std::unique_ptr<juce::XmlElement> root = juce::XmlDocument::parse (getLayoutsFile());

    if (root == nullptr || root->getTagName() != "Layouts")
        root = std::make_unique<juce::XmlElement> ("Layouts");

    const juce::String tag = "Slot" + juce::String (slot);

    if (auto* old = root->getChildByName (tag))
        root->removeChildElement (old, true);

    auto* e = root->createNewChildElement (tag);
    e->setAttribute ("dockVisible", dockVisible ? 1 : 0);
    e->setAttribute ("dockHeight", dockHeight);
    e->setAttribute ("dockTab", static_cast<int> (dock.getCurrentTab()));
    e->setAttribute ("browser", browserVisible ? 1 : 0);
    e->setAttribute ("browserTab", browserTabs.getCurrentTabIndex());
    e->setAttribute ("inspector", inspectorVisible ? 1 : 0);
    e->setAttribute ("patterns", patternsVisible ? 1 : 0);

    getLayoutsFile().getParentDirectory().createDirectory();
    root->writeTo (getLayoutsFile());
}

void MainComponent::loadLayout (int slot)
{
    std::unique_ptr<juce::XmlElement> root = juce::XmlDocument::parse (getLayoutsFile());
    auto* e = root != nullptr ? root->getChildByName ("Slot" + juce::String (slot)) : nullptr;

    if (e == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                "Layout " + juce::String (slot),
                                                "Nothing is saved in this layout yet. Arrange your panels, then use View > Layouts > Save as Layout "
                                                    + juce::String (slot) + ".");
        return;
    }

    dockVisible      = e->getIntAttribute ("dockVisible", 1) != 0;
    dockHeight       = juce::jlimit (150, 900, e->getIntAttribute ("dockHeight", dockHeight));
    browserVisible   = e->getIntAttribute ("browser", 1) != 0;
    inspectorVisible = e->getIntAttribute ("inspector", 1) != 0;
    patternsVisible  = e->getIntAttribute ("patterns", 1) != 0;

    browserTabs.setCurrentTabIndex (juce::jlimit (0, juce::jmax (0, browserTabs.getNumTabs() - 1),
                                                  e->getIntAttribute ("browserTab", 0)));

    // Only Rack and Mixer come back; the other dock tabs need a clip or a track to show
    const int tab = e->getIntAttribute ("dockTab", -1);

    if (dockVisible && session != nullptr)
    {
        if (tab == static_cast<int> (DockPanel::mixerTab))
            dock.showTab (DockPanel::mixerTab);
        else if (tab == static_cast<int> (DockPanel::rackTab))
            dock.showTab (DockPanel::rackTab);
    }

    resized();
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
