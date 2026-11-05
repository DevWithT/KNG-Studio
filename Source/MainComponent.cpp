#include "MainComponent.h"

MainComponent::MainComponent (tracktion::Engine& engineToUse)
    : engine (engineToUse)
{
    addAndMakeVisible (settingsButton);
    settingsButton.onClick = [this] { showAudioSettings(); };

    addAndMakeVisible (deviceLabel);
    deviceLabel.setJustificationType (juce::Justification::centredLeft);

    session = std::make_unique<EditSession> (engine);

    transportBar = std::make_unique<TransportBar> (*session);
    addAndMakeVisible (*transportBar);

    trackLane = std::make_unique<TrackLane> (*session);
    addAndMakeVisible (*trackLane);

    engine.getDeviceManager().deviceManager.addChangeListener (this);
    updateDeviceLabel();

    setSize (1000, 650);
}

MainComponent::~MainComponent()
{
    engine.getDeviceManager().deviceManager.removeChangeListener (this);
    trackLane = nullptr;
    transportBar = nullptr;
    session = nullptr;
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto topBar = area.removeFromTop (30);

    settingsButton.setBounds (topBar.removeFromLeft (130));
    topBar.removeFromLeft (10);
    deviceLabel.setBounds (topBar);

    area.removeFromTop (10);

    if (transportBar != nullptr)
        transportBar->setBounds (area.removeFromTop (40));

    area.removeFromTop (10);

    if (trackLane != nullptr)
        trackLane->setBounds (area.removeFromTop (120));
}

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
    options.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}
