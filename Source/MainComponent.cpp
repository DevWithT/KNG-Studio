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
    engine.getPluginManager().createBuiltInType<KngLoudnessPlugin>();
    engine.getPluginManager().createBuiltInType<KngFattenerPlugin>();
    engine.getPluginManager().createBuiltInType<Kng808Plugin>();
    engine.getPluginManager().createBuiltInType<KngAirPlugin>();
    engine.getPluginManager().createBuiltInType<KngGatePlugin>();
    engine.getPluginManager().createBuiltInType<KngWidthPlugin>();
    engine.getPluginManager().createBuiltInType<KngRadioPlugin>();
    engine.getPluginManager().createBuiltInType<KngThrowPlugin>();

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
    kitBrowser->getProjectBpm = [this]
    {
        return session != nullptr ? session->getBpmAt (session->getTransport().getPosition().inSeconds()) : 0.0;
    };
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

    // Track inspector down the right side, full height
    if (inspector != nullptr)
    {
        inspector->setVisible (inspectorVisible);

        if (inspectorVisible)
        {
            inspector->setBounds (area.removeFromRight (300));
            area.removeFromRight (8);
        }
    }

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

    // Patterns list next to the timeline
    if (patternList != nullptr)
    {
        patternList->setVisible (patternsVisible);

        if (patternsVisible)
        {
            patternList->setBounds (area.removeFromLeft (180));
            area.removeFromLeft (8);
        }
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

    // The Playlist tools take the row the old Patterns strip used
    auto patternRow = area.removeFromTop (24);

    if (playlistToolbar != nullptr)
        playlistToolbar->setBounds (patternRow.reduced (0, 1));

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
