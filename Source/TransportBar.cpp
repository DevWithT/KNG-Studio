#include "TransportBar.h"

TransportBar::TransportBar (EditSession& sessionToUse)
    : session (sessionToUse)
{
    for (auto* b : { &playButton, &stopButton, &recordButton })
        addAndMakeVisible (b);

    addAndMakeVisible (armButton);
    addAndMakeVisible (timeLabel);

    playButton.onClick   = [this] { session.play(); };
    stopButton.onClick   = [this] { session.stop(); };
    recordButton.onClick = [this] { session.record(); };
    armButton.onClick    = [this] { session.setArmed (armButton.getToggleState()); };

    armButton.setToggleState (session.isArmed(), juce::dontSendNotification);

    recordButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red);
    playButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::green);

    timeLabel.setJustificationType (juce::Justification::centred);
    timeLabel.setFont (juce::Font (juce::FontOptions (20.0f)));
    timeLabel.setText (formatTime (0.0), juce::dontSendNotification);

    startTimerHz (30);
}

TransportBar::~TransportBar()
{
    stopTimer();
}

void TransportBar::resized()
{
    auto area = getLocalBounds();

    playButton.setBounds (area.removeFromLeft (80).reduced (2));
    stopButton.setBounds (area.removeFromLeft (80).reduced (2));
    recordButton.setBounds (area.removeFromLeft (80).reduced (2));
    area.removeFromLeft (10);
    armButton.setBounds (area.removeFromLeft (110).reduced (2));
    timeLabel.setBounds (area.removeFromLeft (140));
}

void TransportBar::timerCallback()
{
    auto& transport = session.getTransport();

    timeLabel.setText (formatTime (transport.getPosition().inSeconds()), juce::dontSendNotification);
    recordButton.setToggleState (transport.isRecording(), juce::dontSendNotification);
    playButton.setToggleState (transport.isPlaying() && ! transport.isRecording(), juce::dontSendNotification);
    armButton.setToggleState (session.isArmed(), juce::dontSendNotification);
}

juce::String TransportBar::formatTime (double seconds)
{
    const int totalMs = (int) (seconds * 1000.0);
    const int mins = totalMs / 60000;
    const int secs = (totalMs / 1000) % 60;
    const int ms   = totalMs % 1000;

    return juce::String::formatted ("%02d:%02d.%03d", mins, secs, ms);
}
