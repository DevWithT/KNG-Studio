#include "TrackHeader.h"

TrackHeader::TrackHeader (EditSession& sessionToUse, int trackIndexToUse)
    : session (sessionToUse),
      trackIndex (trackIndexToUse),
      isBus (sessionToUse.isReverbBus (trackIndexToUse))
{
    addAndMakeVisible (nameLabel);
    nameLabel.setFont (juce::Font (juce::FontOptions (15.0f)));
    nameLabel.setInterceptsMouseClicks (false, false);
    nameLabel.onTextChange = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
        {
            const auto newName = nameLabel.getText().trim();

            if (newName.isNotEmpty())
            {
                session.beginEdit();
                t->setName (newName);
                session.save();
            }
        }
    };

    for (auto* b : { &armButton, &muteButton, &soloButton, &fxButton })
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

    fxButton.onClick = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
            if (session.onShowFxChain)
                session.onShowFxChain (t->pluginList, t->getName() + " FX");
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

    addAndMakeVisible (sendLabel);
    sendLabel.setText ("Reverb", juce::dontSendNotification);
    sendLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    sendLabel.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (sendSlider);
    sendSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sendSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    sendSlider.setRange (-60.0, 0.0, 0.1);
    sendSlider.setDoubleClickReturnValue (true, -60.0);
    sendSlider.textFromValueFunction = [] (double v)
    {
        return v <= -60.0 ? juce::String ("Off") : juce::String (v, 1) + " dB";
    };
    sendSlider.setValue (session.getSendDb (trackIndex), juce::dontSendNotification);
    sendSlider.onValueChange = [this] { session.setSendDb (trackIndex, (float) sendSlider.getValue()); };

    addAndMakeVisible (meter);

    if (auto* t = session.getTrack (trackIndex))
    {
        nameLabel.setText (t->getName(), juce::dontSendNotification);
        meter.attachTo (t->getLevelMeterPlugin());

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

    // The reverb bus doesn't record and can't send to itself
    if (isBus)
    {
        armButton.setVisible (false);
        inputBox.setVisible (false);
        sendLabel.setVisible (false);
        sendSlider.setVisible (false);
    }

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

        // Picks up undo/redo of a rename
        if (! nameLabel.isBeingEdited() && nameLabel.getText() != t->getName())
            nameLabel.setText (t->getName(), juce::dontSendNotification);
    }

    if (! inputBox.isPopupActive() && inputBox.getNumItems() > 0)
        inputBox.setSelectedId (session.getTrackInput (trackIndex) + 1, juce::dontSendNotification);

    repaint();
}

void TrackHeader::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (isBus ? juce::Colour (0xff3b3548) : juce::Colour (0xff3a3f44));
    g.fillRoundedRectangle (r, 4.0f);

    // Track colour strip down the left edge
    if (auto* t = session.getTrack (trackIndex))
    {
        const auto c = t->getColour();

        if (c.getAlpha() > 0)
        {
            g.setColour (c.withAlpha (1.0f));
            g.fillRoundedRectangle (r.withWidth (5.0f), 2.0f);
        }
    }

    if (session.isArmed (trackIndex))
    {
        g.setColour (juce::Colours::red.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 2.0f);
    }
}

void TrackHeader::resized()
{
    auto r = getLocalBounds().reduced (6);
    r.removeFromLeft (4);   // room for the colour strip

    meter.setBounds (r.removeFromRight (8));
    r.removeFromRight (4);

    auto row1 = r.removeFromTop (22);
    fxButton.setBounds (row1.removeFromRight (32));
    row1.removeFromRight (3);
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
    panSlider.setBounds (row3.removeFromRight (60));
    row3.removeFromRight (6);
    volumeSlider.setBounds (row3);

    r.removeFromTop (4);
    auto row4 = r.removeFromTop (22);
    sendLabel.setBounds (row4.removeFromLeft (50));
    sendSlider.setBounds (row4);
}

void TrackHeader::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (nameLabel.getBounds().contains (e.getPosition()))
        nameLabel.showEditor();
}

void TrackHeader::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        return;

    auto* s = &session;
    const int idx = trackIndex;
    juce::Component::SafePointer<TrackHeader> safe (this);

    juce::PopupMenu menu;

    menu.addItem ("Rename", [safe] { if (safe != nullptr) safe->nameLabel.showEditor(); });

    menu.addItem ("Move Up", idx > 0, false,
                  [s, idx] { juce::MessageManager::callAsync ([s, idx] { s->moveTrack (idx, -1); }); });

    menu.addItem ("Move Down", idx < session.getNumTracks() - 1, false,
                  [s, idx] { juce::MessageManager::callAsync ([s, idx] { s->moveTrack (idx, 1); }); });

    // Colour palette
    struct Swatch { const char* name; juce::uint32 argb; };
    const Swatch swatches[] = {
        { "Blue",   0xff3a6ea5 }, { "Red",    0xffc0392b }, { "Orange", 0xffd35400 },
        { "Yellow", 0xffc9a227 }, { "Green",  0xff2e8b57 }, { "Teal",   0xff16a085 },
        { "Purple", 0xff8e44ad }, { "Pink",   0xffc2185b }, { "Grey",   0xff607d8b }
    };

    juce::PopupMenu colours;

    for (auto& sw : swatches)
    {
        const juce::Colour c (sw.argb);
        colours.addItem (sw.name, [s, idx, c]
        {
            if (auto* t = s->getTrack (idx))
            {
                s->beginEdit();
                t->setColour (c);
                s->save();
            }
        });
    }

    menu.addSubMenu ("Color", colours);
    menu.addSeparator();

    // Deleting rebuilds the track list, so do it after this header's click finishes
    menu.addItem ("Delete Track", [s, idx]
    {
        juce::MessageManager::callAsync ([s, idx] { s->deleteTrack (idx); });
    });

    menu.showMenuAsync ({});
}
