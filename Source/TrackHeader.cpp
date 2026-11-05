#include "TrackHeader.h"

TrackHeader::TrackHeader (EditSession& sessionToUse, int trackIndexToUse)
    : session (sessionToUse), trackIndex (trackIndexToUse)
{
    addAndMakeVisible (nameLabel);
    nameLabel.setFont (juce::Font (juce::FontOptions (15.0f)));

    for (auto* b : { &armButton, &muteButton, &soloButton })
    {
        addAndMakeVisible (b);
        b->setClickingTogglesState (false);
    }

    armButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red);
    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::orange);
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::gold);

    armButton.onClick = [this]
    {
        session.setArmed (trackIndex, ! session.isArmed (trackIndex));
    };

    muteButton.onClick = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
            t->setMute (! t->isMuted (false));
    };

    soloButton.onClick = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
            t->setSolo (! t->isSolo (false));
    };

    addAndMakeVisible (inputBox);
    inputBox.setTextWhenNoChoicesAvailable ("No inputs");
    inputBox.onChange = [this]
    {
        const int id = inputBox.getSelectedId();

        if (id > 0)
            session.setTrackInput (trackIndex, id - 1);
    };
    refreshInputList();

    addAndMakeVisible (volumeSlider);
    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    volumeSlider.setRange (-60.0, 6.0, 0.1);
    volumeSlider.setTextValueSuffix (" dB");
    volumeSlider.setDoubleClickReturnValue (true, 0.0);

    addAndMakeVisible (panSlider);
    panSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange (-1.0, 1.0, 0.01);
    panSlider.setDoubleClickReturnValue (true, 0.0);

    if (auto* t = session.getTrack (trackIndex))
    {
        nameLabel.setText (t->getName(), juce::dontSendNotification);

        if (auto* vp = t->getVolumePlugin())
        {
            volumeSlider.setValue (vp->getVolumeDb(), juce::dontSendNotification);
            panSlider.setValue (vp->getPan(), juce::dontSendNotification);
        }
    }

    volumeSlider.onValueChange = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
            if (auto* vp = t->getVolumePlugin())
                vp->setVolumeDb ((float) volumeSlider.getValue());
    };

    panSlider.onValueChange = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
            if (auto* vp = t->getVolumePlugin())
                vp->setPan ((float) panSlider.getValue());
    };

    startTimerHz (10);
}

TrackHeader::~TrackHeader()
{
    stopTimer();
}

void TrackHeader::refreshInputList()
{
    auto names = session.getInputNames();

    if (names == lastInputNames)
        return;

    lastInputNames = names;
    inputBox.clear (juce::dontSendNotification);

    for (int i = 0; i < names.size(); ++i)
        inputBox.addItem (names[i], i + 1);

    if (names.size() > 0)
        inputBox.setSelectedId (session.getTrackInput (trackIndex) + 1, juce::dontSendNotification);
}

void TrackHeader::timerCallback()
{
    refreshInputList();

    armButton.setToggleState (session.isArmed (trackIndex), juce::dontSendNotification);

    if (auto* t = session.getTrack (trackIndex))
    {
        muteButton.setToggleState (t->isMuted (false), juce::dontSendNotification);
        soloButton.setToggleState (t->isSolo (false), juce::dontSendNotification);
    }

    if (! inputBox.isPopupActive() && inputBox.getNumItems() > 0)
        inputBox.setSelectedId (session.getTrackInput (trackIndex) + 1, juce::dontSendNotification);

    repaint();
}

void TrackHeader::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff3a3f44));
    g.fillRoundedRectangle (r, 4.0f);

    if (session.isArmed (trackIndex))
    {
        g.setColour (juce::Colours::red.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 2.0f);
    }
}

void TrackHeader::resized()
{
    auto r = getLocalBounds().reduced (6);

    auto row1 = r.removeFromTop (22);
    soloButton.setBounds (row1.removeFromRight (26));
    row1.removeFromRight (3);
    muteButton.setBounds (row1.removeFromRight (26));
    row1.removeFromRight (3);
    armButton.setBounds (row1.removeFromRight (26));
    row1.removeFromRight (6);
    nameLabel.setBounds (row1);

    r.removeFromTop (4);
    inputBox.setBounds (r.removeFromTop (22));

    r.removeFromTop (4);
    auto row3 = r.removeFromTop (22);
    panSlider.setBounds (row3.removeFromRight (70));
    row3.removeFromRight (6);
    volumeSlider.setBounds (row3);
}
