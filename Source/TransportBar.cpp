#include "TransportBar.h"
#include "ExportDialog.h"

TransportBar::TransportBar (EditSession& sessionToUse)
    : session (sessionToUse)
{
    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &stopButton, &recordButton, &timeLabel,
                                                             &tempoSlider, &countInBox, &clickButton,
                                                             &loopButton, &punchButton, &snapButton, &monitorButton, &writeButton })
        addAndMakeVisible (c);

    // Transport
    playButton.onClick   = [this] { session.play(); };
    stopButton.onClick   = [this] { session.stop(); };
    recordButton.onClick = [this] { session.record(); };

    playButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2e8b57));
    recordButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red);
    recordButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff6b6b));

    timeLabel.setJustificationType (juce::Justification::centred);
    timeLabel.setFont (juce::Font (juce::FontOptions (20.0f)));
    timeLabel.setText (formatTime (0.0), juce::dontSendNotification);

    addAndMakeVisible (barLabel);
    barLabel.setJustificationType (juce::Justification::centred);
    barLabel.setFont (juce::Font (juce::FontOptions (15.0f)));
    barLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    barLabel.setTooltip ("Bar and beat at the playhead");

    addAndMakeVisible (timeSigButton);
    timeSigButton.setTooltip ("Time signature at the playhead. Click to change it from this bar on.");
    timeSigButton.onClick = [this]
    {
        auto* s = &session;
        const double t = session.getTransport().getPosition().inSeconds();
        juce::PopupMenu menu;

        for (auto* sig : { "4/4", "3/4", "6/8", "5/4", "7/8", "12/8", "2/4" })
        {
            menu.addItem (sig, [s, t, sig]
            {
                const auto parts = juce::StringArray::fromTokens (sig, "/", "");
                s->addTimeSigChange (t, parts[0].getIntValue(), parts[1].getIntValue());
            });
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&timeSigButton));
    };

    // Tempo
    tempoSlider.setSliderStyle (juce::Slider::IncDecButtons);
    tempoSlider.setRange (40.0, 300.0, 1.0);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 70, 24);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.setValue (session.getBpm(), juce::dontSendNotification);
    tempoSlider.onValueChange = [this] { session.setBpmAt (session.getTransport().getPosition().inSeconds(), tempoSlider.getValue()); };

    // Count-in
    countInBox.addItem ("No count-in", 1);
    countInBox.addItem ("1 bar count-in", 2);
    countInBox.addItem ("2 bar count-in", 3);
    countInBox.setSelectedId (session.getCountInBars() + 1, juce::dontSendNotification);
    countInBox.onChange = [this] { session.setCountInBars (countInBox.getSelectedId() - 1); };

    // Toggles
    clickButton.onClick   = [this] { session.setClickEnabled (clickButton.getToggleState()); };
    loopButton.onClick    = [this] { session.setLooping (loopButton.getToggleState()); };
    punchButton.onClick   = [this] { session.setPunchEnabled (punchButton.getToggleState()); };
    snapButton.onClick    = [this] { session.setSnapEnabled (snapButton.getToggleState()); };
    monitorButton.onClick = [this] { session.setMonitoring (monitorButton.getToggleState()); };
    writeButton.onClick   = [this] { session.setAutomationWrite (writeButton.getToggleState()); };

    addAndMakeVisible (learnButton);
    learnButton.setClickingTogglesState (false);
    learnButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffc0392b));
    learnButton.setTooltip ("MIDI Learn (Ctrl+L): move a knob here, then twist one on your controller to link them");
    learnButton.onClick = [this] { session.setMidiLearn (! session.isMidiLearn()); };

    // Tooltips
    playButton.setTooltip ("Play / Pause (Space)");
    stopButton.setTooltip ("Stop and return to where you started (Space)");
    recordButton.setTooltip ("Record on armed tracks");
    tempoSlider.setTooltip ("Tempo at the playhead. Right-click the ruler to add a tempo change.");
    countInBox.setTooltip ("Bars of click before recording starts");
    clickButton.setTooltip ("Metronome on or off");
    loopButton.setTooltip ("Loop the region marked on the ruler");
    punchButton.setTooltip ("Only record between the loop markers");
    snapButton.setTooltip ("Snap clips and markers to the beat grid (hold Alt to ignore)");
    monitorButton.setTooltip ("Hear your inputs live while armed");
    writeButton.setTooltip ("Record fader and knob moves as automation while playing");

    timerCallback();
    startTimerHz (30);
}

TransportBar::~TransportBar()
{
    stopTimer();

    // The Export window reads the song, so it closes with it (this also cancels a running export)
    if (exportWindow != nullptr)
        delete exportWindow.getComponent();
}

//==============================================================================
void TransportBar::importBeat()
{
    chooser = std::make_unique<juce::FileChooser> ("Import audio",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.mp3;*.aif;*.aiff;*.flac;*.ogg");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();

                              if (file.existsAsFile() && ! session.importAudioFile (file))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                          "Import failed",
                                                                          "Couldn't read that file.");
                          });
}

void TransportBar::showExportDialog()
{
    // Already open: bring it forward instead of opening a second one
    if (exportWindow != nullptr)
    {
        exportWindow->toFront (true);
        return;
    }

    auto* dialog = new ExportDialog (session);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (dialog);
    options.dialogTitle = "Export";
    options.dialogBackgroundColour = juce::Colour (0xff2b2f33);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    exportWindow = options.launchAsync();
}

//==============================================================================
void TransportBar::timerCallback()
{
    auto& transport = session.getTransport();

    timeLabel.setText (formatTime (transport.getPosition().inSeconds()), juce::dontSendNotification);
    recordButton.setToggleState (transport.isRecording(), juce::dontSendNotification);
    playButton.setToggleState (transport.isPlaying() && ! transport.isRecording(), juce::dontSendNotification);

    clickButton.setToggleState (session.isClickEnabled(), juce::dontSendNotification);
    loopButton.setToggleState (session.isLooping(), juce::dontSendNotification);
    punchButton.setToggleState (session.isPunchEnabled(), juce::dontSendNotification);
    snapButton.setToggleState (session.isSnapEnabled(), juce::dontSendNotification);
    monitorButton.setToggleState (session.isMonitoring(), juce::dontSendNotification);
    writeButton.setToggleState (session.isAutomationWrite(), juce::dontSendNotification);
    learnButton.setToggleState (session.isMidiLearn(), juce::dontSendNotification);

    if (! countInBox.isPopupActive())
        countInBox.setSelectedId (session.getCountInBars() + 1, juce::dontSendNotification);

    const double now = transport.getPosition().inSeconds();

    if (! tempoSlider.isMouseButtonDown())
        tempoSlider.setValue (session.getBpmAt (now), juce::dontSendNotification);

    const auto sig = session.getTimeSigAt (now);

    if (timeSigButton.getButtonText() != sig)
        timeSigButton.setButtonText (sig);

    // Bar.beat, counted on the tempo map
    auto& ts = session.getEdit().tempoSequence;
    const double barStart = session.barStartAt (now);
    const int beatInBar = (int) std::floor (ts.toBeats (tracktion::TimePosition::fromSeconds (now)).inBeats()
                                            - ts.toBeats (tracktion::TimePosition::fromSeconds (barStart)).inBeats() + 1.0e-6) + 1;
    barLabel.setText (juce::String (session.barNumberAt (now)) + "." + juce::String (beatInBar), juce::dontSendNotification);
}

juce::String TransportBar::formatTime (double seconds)
{
    const int totalMs = (int) (seconds * 1000.0);
    const int mins = totalMs / 60000;
    const int secs = (totalMs / 1000) % 60;
    const int ms   = totalMs % 1000;

    return juce::String::formatted ("%02d:%02d.%03d", mins, secs, ms);
}

//==============================================================================
void TransportBar::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff24282c));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));

    for (int x : dividers)
        g.drawVerticalLine (x, 6.0f, (float) getHeight() - 6.0f);
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (6, 4);
    dividers.clear();

    auto place = [&r] (juce::Component& c, int w)
    {
        c.setBounds (r.removeFromLeft (w));
        r.removeFromLeft (4);
    };

    auto divider = [this, &r]
    {
        r.removeFromLeft (4);
        dividers.add (r.getX());
        r.removeFromLeft (10);
    };

    place (playButton, 64);
    place (stopButton, 64);
    place (recordButton, 64);
    divider();

    place (timeLabel, 130);
    place (barLabel, 64);
    divider();

    place (tempoSlider, 150);
    place (timeSigButton, 56);
    place (countInBox, 130);
    place (clickButton, 70);
    divider();

    place (loopButton, 64);
    place (punchButton, 70);
    place (snapButton, 64);
    divider();

    place (monitorButton, 90);
    place (writeButton, 100);
    place (learnButton, 64);
}
