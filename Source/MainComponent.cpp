#include "MainComponent.h"

MainComponent::MainComponent (tracktion::Engine& engineToUse)
    : engine (engineToUse)
{
    setWantsKeyboardFocus (true);

    for (auto* b : { &settingsButton, &scanButton, &newButton, &openButton, &saveButton })
        addAndMakeVisible (b);

    settingsButton.onClick = [this] { showAudioSettings(); };
    scanButton.onClick     = [this] { showPluginScanner(); };
    newButton.onClick      = [this] { newProject(); };
    openButton.onClick     = [this] { openProject(); };
    saveButton.onClick     = [this] { saveProject(); };

    addAndMakeVisible (projectLabel);
    projectLabel.setFont (juce::Font (juce::FontOptions (16.0f)));
    projectLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (deviceLabel);
    deviceLabel.setJustificationType (juce::Justification::centredLeft);

    tracksViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (tracksViewport);

    engine.getDeviceManager().deviceManager.addChangeListener (this);
    updateDeviceLabel();

    loadProject (getLastProjectFile());

    setSize (1280, 800);
}

MainComponent::~MainComponent()
{
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
    session->onShowFxChain = [this] (tracktion::PluginList& list, const juce::String& title)
    {
        showFxChain (list, title);
    };

    transportBar = std::make_unique<TransportBar> (*session);
    addAndMakeVisible (*transportBar);

    ruler = std::make_unique<TimelineRuler> (*session);
    addAndMakeVisible (*ruler);

    masterStrip = std::make_unique<MasterStrip> (*session);
    addAndMakeVisible (*masterStrip);

    tracksView = std::make_unique<TracksView> (*session);
    tracksViewport.setViewedComponent (tracksView.get(), false);

    projectLabel.setText (session->getProjectName(), juce::dontSendNotification);
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
void MainComponent::showFxChain (tracktion::PluginList& list, const juce::String& title)
{
    if (session == nullptr)
        return;

    // Reuse the window if this chain is already open
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
    options.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto topBar = area.removeFromTop (30);

    settingsButton.setBounds (topBar.removeFromLeft (130));
    topBar.removeFromLeft (6);
    scanButton.setBounds (topBar.removeFromLeft (110));
    topBar.removeFromLeft (10);
    newButton.setBounds (topBar.removeFromLeft (60));
    topBar.removeFromLeft (4);
    openButton.setBounds (topBar.removeFromLeft (60));
    topBar.removeFromLeft (4);
    saveButton.setBounds (topBar.removeFromLeft (60));
    topBar.removeFromLeft (12);
    projectLabel.setBounds (topBar.removeFromLeft (200));
    topBar.removeFromLeft (10);
    deviceLabel.setBounds (topBar);

    area.removeFromTop (10);

    if (transportBar != nullptr)
        transportBar->setBounds (area.removeFromTop (76));

    area.removeFromTop (10);

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

        if (code == 'Y' || code == 'y')  { session->redo(); return true; }
        if (code == 'S' || code == 's')  { saveProject();   return true; }
        if (code == 'O' || code == 'o')  { openProject();   return true; }
        if (code == 'N' || code == 'n')  { newProject();    return true; }
        if (code == 'D' || code == 'd')  { session->duplicateSelectedClip(); return true; }
        if (code == 'C' || code == 'c')  { session->copySelectedClip();      return true; }
        if (code == 'V' || code == 'v')  { session->pasteClip();             return true; }
    }

    // Space toggles play/stop
    if (code == juce::KeyPress::spaceKey)
    {
        if (session->getTransport().isPlaying())
            session->stop();
        else
            session->play();

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
