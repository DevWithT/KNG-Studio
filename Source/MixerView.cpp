#include "MixerView.h"
#include "VolumeHelpers.h"
#include "PluginBrowser.h"
#include "GenericPluginPanel.h"
#include "KngTheme.h"
#include "RouteMenu.h"
#include "RoutePopup.h"
#include "TrackWidth.h"
#include <map>

namespace te = tracktion;

//==============================================================================
MixerStrip::MixerStrip (EditSession& sessionToUse, int trackIndexToUse)
    : session (sessionToUse), trackIndex (trackIndexToUse)
{
    isBus = ! isMaster() && session.isReverbBus (trackIndex);

    for (auto* c : std::initializer_list<juce::Component*> { &nameLabel, &fxButton, &muteButton, &soloButton,
                                                             &sendLabel, &panLabel, &panKnob,
                                                             &fader, &meter, &outputButton, &widthLabel, &widthKnob })
        addAndMakeVisible (c);

    addAndMakeVisible (addFxButton);
    addFxButton.setTooltip ("Add an effect to this channel");
    addFxButton.onClick = [this] { showAddFxMenu(); };

    addChildComponent (moreFxButton);
    moreFxButton.setTooltip ("More effects on this channel");
    moreFxButton.onClick = [this] { showMoreMenu(); };

    outputButton.setTooltip ("Where this channel's sound goes: Master, or another track such as a bus");
    outputButton.onClick = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
        {
            juce::PopupMenu menu;
            KngRoute::addItems (menu, session, *t);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&outputButton));
        }
    };

    addAndMakeVisible (routeButton);
    routeButton.setTooltip ("Sends and routing: reverb, delay or headphone sends, and where this channel's sound goes");
    routeButton.setColour (juce::TextButton::buttonOnColourId, KngTheme::accentSoft);
    routeButton.setColour (juce::TextButton::textColourOnId, KngTheme::text);
    routeButton.onClick = [this] { RoutePopup::show (session, trackIndex, routeButton); };

    sendLabel.setVisible (false);
    fxButton.setVisible (false);

    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
    nameLabel.setColour (juce::Label::textColourId, KngTheme::ink);

    for (auto* l : { &sendLabel, &panLabel, &widthLabel })
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
    muteButton.setColour (juce::TextButton::buttonOnColourId, KngTheme::accent);
    soloButton.setColour (juce::TextButton::buttonOnColourId, KngTheme::laneYellow);

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
    for (auto* k : { &panKnob, &widthKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->setPopupDisplayEnabled (true, true, this);
    }

    panKnob.setRange (-1.0, 1.0, 0.01);
    panKnob.setDoubleClickReturnValue (true, 0.0);
    panKnob.onValueChange = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
            if (auto* vp = t->getVolumePlugin())
                vp->setPan ((float) panKnob.getValue());
    };

    widthKnob.setRange (0.0, 2.0, 0.01);
    widthKnob.setDoubleClickReturnValue (true, 1.0);
    widthKnob.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    widthKnob.onDragStart   = [this] { session.beginEdit(); };
    widthKnob.onValueChange = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
            KngTrackWidth::set (session, *t, (float) widthKnob.getValue());
    };
    widthKnob.onDragEnd     = [this] { session.save(); };

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
        outputButton.setVisible (false);
        widthKnob.setVisible (false);
        widthLabel.setVisible (false);
        meter.attachTo (session.getMasterMeterPlugin());
    }
    else if (auto* t = session.getTrack (trackIndex))
    {
        meter.attachTo (t->getLevelMeterPlugin());
    }

    if (isMaster() || isBus)
    {
        sendLabel.setVisible (false);
        routeButton.setVisible (false);
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
    rebuildInserts();

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

    const auto outText = "Out: " + KngRoute::currentName (*t);

    if (outputButton.getButtonText() != outText)
        outputButton.setButtonText (outText);

    if (auto* vp = t->getVolumePlugin())
    {
        if (! fader.isMouseButtonDown())
            fader.setValue (session.getTrackVolumeDb (trackIndex), juce::dontSendNotification);

        if (! panKnob.isMouseButtonDown())
            panKnob.setValue (vp->getPan(), juce::dontSendNotification);
    }

    if (! widthKnob.isMouseButtonDown())
        widthKnob.setValue (KngTrackWidth::get (*t), juce::dontSendNotification);

    if (! isBus)
    {
        const int numSends = session.getSendBuses (trackIndex).size();
        const auto sendsText = numSends > 0 ? "Sends " + juce::String (numSends) : juce::String ("Sends");

        if (routeButton.getButtonText() != sendsText)
            routeButton.setButtonText (sendsText);

        routeButton.setToggleState (numSends > 0, juce::dontSendNotification);
    }

    repaint();
}

void MixerStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (KngTheme::panel);
    g.fillRect (r);

    // Header band: the track's color (accent for the master), with the name on it
    juce::Colour band = isMaster() ? KngTheme::accent
                      : isBus      ? juce::Colour (0xff8c84a6)
                                   : KngTheme::laneGrey;

    if (! isMaster())
        if (auto* t = session.getTrack (trackIndex))
            if (t->getColour().getAlpha() > 0)
                band = t->getColour().withAlpha (1.0f);

    g.setColour (band);
    g.fillRect (r.withHeight (32.0f));

    g.setColour (KngTheme::line);
    g.drawRect (r, 1.0f);
}

void MixerStrip::resized()
{
    auto r = getLocalBounds().reduced (4);
    r.removeFromTop (6);

    nameLabel.setBounds (r.removeFromTop (20));
    r.removeFromTop (2);

    // Inserts: the first four, then + FX. With more than four, the fourth slot becomes "+N more".
    const int shown = insertButtons.size() > 4 ? 3 : insertButtons.size();
    for (int i = 0; i < insertButtons.size(); ++i)
    {
        insertButtons[i]->setVisible (i < shown);
        insertButtons[i]->slotsUp = juce::jmin (i, juce::jmax (0, shown - 1));
        insertButtons[i]->slotsDown = juce::jmax (0, shown - 1 - i);

        if (i < shown)
        {
            insertButtons[i]->setBounds (r.removeFromTop (18).reduced (2, 0));
            r.removeFromTop (2);
        }
    }

    // The effects that did not fit sit behind one button
    moreFxButton.setVisible (insertButtons.size() > 4);

    if (insertButtons.size() > 4)
    {
        moreFxButton.setButtonText ("+" + juce::String (insertButtons.size() - shown) + " more");
        moreFxButton.setBounds (r.removeFromTop (18).reduced (2, 0));
        r.removeFromTop (2);
    }

    addFxButton.setBounds (r.removeFromTop (20).reduced (2, 0));
    r.removeFromTop (6);

    routeButton.setBounds (r.removeFromTop (22).reduced (2, 0));
    r.removeFromTop (6);

    auto panArea = r.removeFromTop (50);
    auto widthArea = panArea.removeFromRight (panArea.getWidth() / 2);
    panLabel.setBounds (panArea.removeFromTop (14));
    panKnob.setBounds (panArea.withSizeKeepingCentre (32, 32));
    widthLabel.setBounds (widthArea.removeFromTop (14));
    widthKnob.setBounds (widthArea.withSizeKeepingCentre (32, 32));

    outputButton.setBounds (r.removeFromTop (20).reduced (2, 0));
    r.removeFromTop (4);

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
    g.fillAll (KngTheme::panel2);
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
    : DocumentWindow ("Mixer", KngTheme::bg, DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new MixerView (session), true);
    setResizable (true, false);
    centreWithSize (900, 520);
    setVisible (true);
}
