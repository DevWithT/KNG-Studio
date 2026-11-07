#include "MixerView.h"
#include "VolumeHelpers.h"

namespace te = tracktion;

//==============================================================================
MixerStrip::MixerStrip (EditSession& sessionToUse, int trackIndexToUse)
    : session (sessionToUse), trackIndex (trackIndexToUse)
{
    isBus = ! isMaster() && session.isReverbBus (trackIndex);

    for (auto* c : std::initializer_list<juce::Component*> { &nameLabel, &fxButton, &muteButton, &soloButton,
                                                             &sendLabel, &panLabel, &sendKnob, &panKnob,
                                                             &fader, &meter })
        addAndMakeVisible (c);

    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (juce::Font (juce::FontOptions (13.0f)));

    for (auto* l : { &sendLabel, &panLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::Font (juce::FontOptions (11.0f)));
    }

    // FX
    fxButton.onClick = [this]
    {
        if (! session.onShowFxChain)
            return;

        if (isMaster())
            session.onShowFxChain (session.getEdit().getMasterPluginList(), "Master FX");
        else if (auto* t = session.getTrack (trackIndex))
            session.onShowFxChain (t->pluginList, t->getName() + " FX");
    };

    // Mute and solo
    muteButton.setClickingTogglesState (false);
    soloButton.setClickingTogglesState (false);
    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::orange);
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::gold);

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

    // Knobs
    for (auto* k : { &sendKnob, &panKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->setPopupDisplayEnabled (true, true, this);
    }

    sendKnob.setRange (-60.0, 0.0, 0.1);
    sendKnob.setDoubleClickReturnValue (true, -60.0);
    sendKnob.textFromValueFunction = [] (double v)
    {
        return v <= -60.0 ? juce::String ("Off") : juce::String (v, 1) + " dB";
    };
    sendKnob.onValueChange = [this] { session.setSendDb (trackIndex, (float) sendKnob.getValue()); };

    panKnob.setRange (-1.0, 1.0, 0.01);
    panKnob.setDoubleClickReturnValue (true, 0.0);
    panKnob.onValueChange = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
            if (auto* vp = t->getVolumePlugin())
                vp->setPan ((float) panKnob.getValue());
    };

    // Fader
    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    KngVolume::setupSlider (fader);
    fader.onValueChange = [this]
    {
        if (isMaster())
        {
            session.setMasterVolumeDb ((float) fader.getValue());
        }
        else if (auto* t = session.getTrack (trackIndex))
        {
            session.setTrackVolumeDb (trackIndex, (float) fader.getValue());
        }
    };

    // Master and bus don't get everything
    if (isMaster())
    {
        nameLabel.setText ("Master", juce::dontSendNotification);
        muteButton.setVisible (false);
        soloButton.setVisible (false);
        panKnob.setVisible (false);
        panLabel.setVisible (false);
        meter.attachTo (session.getMasterMeterPlugin());
    }
    else if (auto* t = session.getTrack (trackIndex))
    {
        meter.attachTo (t->getLevelMeterPlugin());
    }

    if (isMaster() || isBus)
    {
        sendKnob.setVisible (false);
        sendLabel.setVisible (false);
    }

    timerCallback();
    startTimerHz (10);
}

MixerStrip::~MixerStrip()
{
    stopTimer();
}

void MixerStrip::timerCallback()
{
    // Keep in step with the track headers and automation
    if (isMaster())
    {
        if (! fader.isMouseButtonDown())
            fader.setValue (session.getMasterVolumeDb(), juce::dontSendNotification);

        return;
    }

    auto* t = session.getTrack (trackIndex);

    if (t == nullptr)
        return;

    if (! nameLabel.isBeingEdited() && nameLabel.getText() != t->getName())
        nameLabel.setText (t->getName(), juce::dontSendNotification);

    muteButton.setToggleState (t->isMuted (false), juce::dontSendNotification);
    soloButton.setToggleState (t->isSolo (false), juce::dontSendNotification);

    if (auto* vp = t->getVolumePlugin())
    {
        if (! fader.isMouseButtonDown())
            fader.setValue (session.getTrackVolumeDb (trackIndex), juce::dontSendNotification);

        if (! panKnob.isMouseButtonDown())
            panKnob.setValue (vp->getPan(), juce::dontSendNotification);
    }

    if (! isBus && ! sendKnob.isMouseButtonDown())
        sendKnob.setValue (session.getSendDb (trackIndex), juce::dontSendNotification);

    repaint();
}

void MixerStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (isMaster()                       ? juce::Colour (0xff3a3530)
               : isBus                            ? juce::Colour (0xff3b3548)
               : session.isMidiTrack (trackIndex) ? juce::Colour (0xff34403a)
                                                  : juce::Colour (0xff3a3f44));
    g.fillRoundedRectangle (r, 4.0f);

    // Track colour along the top
    if (! isMaster())
    {
        if (auto* t = session.getTrack (trackIndex))
        {
            const auto c = t->getColour();

            if (c.getAlpha() > 0)
            {
                g.setColour (c.withAlpha (1.0f));
                g.fillRoundedRectangle (r.withHeight (5.0f), 2.0f);
            }
        }
    }
}

void MixerStrip::resized()
{
    auto r = getLocalBounds().reduced (4);
    r.removeFromTop (6);

    nameLabel.setBounds (r.removeFromTop (20));
    fxButton.setBounds (r.removeFromTop (22).reduced (8, 0));
    r.removeFromTop (6);

    auto sendArea = r.removeFromTop (54);
    sendLabel.setBounds (sendArea.removeFromTop (14));
    sendKnob.setBounds (sendArea.withSizeKeepingCentre (40, 40));

    auto panArea = r.removeFromTop (54);
    panLabel.setBounds (panArea.removeFromTop (14));
    panKnob.setBounds (panArea.withSizeKeepingCentre (40, 40));

    auto msRow = r.removeFromBottom (24);
    muteButton.setBounds (msRow.removeFromLeft (msRow.getWidth() / 2).reduced (2, 0));
    soloButton.setBounds (msRow.reduced (2, 0));
    r.removeFromBottom (6);

    meter.setBounds (r.removeFromRight (10));
    r.removeFromRight (4);
    fader.setBounds (r);
}

//==============================================================================
MixerView::MixerView (EditSession& sessionToUse)
    : session (sessionToUse)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);

    masterStrip = std::make_unique<MixerStrip> (session, -1);
    addAndMakeVisible (*masterStrip);

    rebuild();
    startTimerHz (4);

    setSize (900, 520);
}

MixerView::~MixerView()
{
    stopTimer();
    viewport.setViewedComponent (nullptr, false);
    strips.clear();
    masterStrip = nullptr;
}

juce::Array<te::AudioTrack*> MixerView::getCurrentTracks()
{
    juce::Array<te::AudioTrack*> result;

    for (int i = 0; i < session.getNumTracks(); ++i)
        result.add (session.getTrack (i));

    return result;
}

void MixerView::timerCallback()
{
    if (getCurrentTracks() != builtTracks)
        rebuild();
}

void MixerView::rebuild()
{
    strips.clear();
    builtTracks = getCurrentTracks();

    for (int i = 0; i < builtTracks.size(); ++i)
        content.addAndMakeVisible (strips.add (new MixerStrip (session, i)));

    resized();
}

void MixerView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));
}

void MixerView::resized()
{
    auto r = getLocalBounds().reduced (6);

    if (masterStrip != nullptr)
        masterStrip->setBounds (r.removeFromRight (stripWidth));

    r.removeFromRight (10);
    viewport.setBounds (r);

    const int h = juce::jmax (0, viewport.getHeight() - viewport.getScrollBarThickness());
    content.setSize (strips.size() * (stripWidth + stripGap), h);

    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setBounds (i * (stripWidth + stripGap), 0, stripWidth, h);
}

//==============================================================================
MixerWindow::MixerWindow (EditSession& session)
    : DocumentWindow ("Mixer", juce::Colour (0xff1e2124), DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new MixerView (session), true);
    setResizable (true, false);
    centreWithSize (900, 520);
    setVisible (true);
}
