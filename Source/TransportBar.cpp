#include "TransportBar.h"

TransportBar::TransportBar (EditSession& sessionToUse)
    : session (sessionToUse)
{
    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &stopButton, &recordButton,
                                                             &timeLabel, &monitorButton, &clickButton,
                                                             &countInBox, &tempoSlider,
                                                             &undoButton, &redoButton, &loopButton,
                                                             &importButton, &snapButton })
        addAndMakeVisible (c);

    // Transport
    playButton.onClick   = [this] { session.play(); };
    stopButton.onClick   = [this] { session.stop(); };
    recordButton.onClick = [this] { session.record(); };

    recordButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red);
    playButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::green);

    timeLabel.setJustificationType (juce::Justification::centred);
    timeLabel.setFont (juce::Font (juce::FontOptions (20.0f)));
    timeLabel.setText (formatTime (0.0), juce::dontSendNotification);

    // Monitor and click
    monitorButton.setToggleState (session.isMonitoring(), juce::dontSendNotification);
    monitorButton.onClick = [this] { session.setMonitoring (monitorButton.getToggleState()); };

    clickButton.setToggleState (session.isClickEnabled(), juce::dontSendNotification);
    clickButton.onClick = [this] { session.setClickEnabled (clickButton.getToggleState()); };

    // Count-in
    countInBox.addItem ("No count-in", 1);
    countInBox.addItem ("1 bar count-in", 2);
    countInBox.addItem ("2 bar count-in", 3);
    countInBox.setSelectedId (session.getCountInBars() + 1, juce::dontSendNotification);
    countInBox.onChange = [this] { session.setCountInBars (countInBox.getSelectedId() - 1); };

    // Tempo
    tempoSlider.setSliderStyle (juce::Slider::IncDecButtons);
    tempoSlider.setRange (40.0, 300.0, 1.0);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 70, 24);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.setValue (session.getBpm(), juce::dontSendNotification);
    tempoSlider.onValueChange = [this] { session.setBpm (tempoSlider.getValue()); };

    // Row 2
    undoButton.onClick   = [this] { session.undo(); };
    redoButton.onClick   = [this] { session.redo(); };
    importButton.onClick = [this] { importBeat(); };

    loopButton.setToggleState (session.isLooping(), juce::dontSendNotification);
    loopButton.onClick = [this] { session.setLooping (loopButton.getToggleState()); };

    snapButton.setToggleState (session.isSnapEnabled(), juce::dontSendNotification);
    snapButton.onClick = [this] { session.setSnapEnabled (snapButton.getToggleState()); };

    startTimerHz (30);
}

TransportBar::~TransportBar()
{
    stopTimer();
}

void TransportBar::importBeat()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a beat",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.mp3;*.aif;*.aiff;*.flac");

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

void TransportBar::resized()
{
    auto area = getLocalBounds();
    auto row1 = area.removeFromTop (36);
    area.removeFromTop (4);
    auto row2 = area.removeFromTop (36);

    playButton.setBounds (row1.removeFromLeft (80).reduced (2));
    stopButton.setBounds (row1.removeFromLeft (80).reduced (2));
    recordButton.setBounds (row1.removeFromLeft (80).reduced (2));
    row1.removeFromLeft (10);
    timeLabel.setBounds (row1.removeFromLeft (140));
    row1.removeFromLeft (10);
    monitorButton.setBounds (row1.removeFromLeft (90).reduced (2));
    clickButton.setBounds (row1.removeFromLeft (70).reduced (2));
    row1.removeFromLeft (6);
    countInBox.setBounds (row1.removeFromLeft (140).reduced (2));
    row1.removeFromLeft (10);
    tempoSlider.setBounds (row1.removeFromLeft (150).reduced (2));

    undoButton.setBounds (row2.removeFromLeft (80).reduced (2));
    redoButton.setBounds (row2.removeFromLeft (80).reduced (2));
    row2.removeFromLeft (10);
    loopButton.setBounds (row2.removeFromLeft (70).reduced (2));
    snapButton.setBounds (row2.removeFromLeft (70).reduced (2));
    row2.removeFromLeft (10);
    importButton.setBounds (row2.removeFromLeft (120).reduced (2));
}

void TransportBar::timerCallback()
{
    auto& transport = session.getTransport();

    timeLabel.setText (formatTime (transport.getPosition().inSeconds()), juce::dontSendNotification);
    recordButton.setToggleState (transport.isRecording(), juce::dontSendNotification);
    playButton.setToggleState (transport.isPlaying() && ! transport.isRecording(), juce::dontSendNotification);

    monitorButton.setToggleState (session.isMonitoring(), juce::dontSendNotification);
    clickButton.setToggleState (session.isClickEnabled(), juce::dontSendNotification);
    loopButton.setToggleState (session.isLooping(), juce::dontSendNotification);
    snapButton.setToggleState (session.isSnapEnabled(), juce::dontSendNotification);

    if (! countInBox.isPopupActive())
        countInBox.setSelectedId (session.getCountInBars() + 1, juce::dontSendNotification);

    if (! tempoSlider.isMouseButtonDown())
        tempoSlider.setValue (session.getBpm(), juce::dontSendNotification);
}

juce::String TransportBar::formatTime (double seconds)
{
    const int totalMs = (int) (seconds * 1000.0);
    const int mins = totalMs / 60000;
    const int secs = (totalMs / 1000) % 60;
    const int ms   = totalMs % 1000;

    return juce::String::formatted ("%02d:%02d.%03d", mins, secs, ms);
}
