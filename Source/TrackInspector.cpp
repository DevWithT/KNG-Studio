#include "TrackInspector.h"
#include <map>
#include "PluginBrowser.h"
#include "VolumeHelpers.h"
#include "KngSamplerPlugin.h"
#include "KngSynthPlugin.h"

namespace te = tracktion;

static void styleHeading (juce::Label& l)
{
    l.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    l.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.45f));
    l.setInterceptsMouseClicks (false, false);
}

TrackInspector::TrackInspector (EditSession& sessionToUse)
    : session (sessionToUse)
{
    for (auto* c : std::initializer_list<juce::Component*> { &nameLabel, &hintLabel, &instHeading, &fxHeading, &sendHeading,
                                                             &instrumentButton, &openInstrumentButton, &addEffectButton,
                                                             &newBusButton, &volumeSlider, &panSlider })
        addAndMakeVisible (c);

    for (auto* l : { &instHeading, &fxHeading, &sendHeading })
        styleHeading (*l);

    nameLabel.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    nameLabel.setInterceptsMouseClicks (false, false);

    hintLabel.setText ("Click a track or a Rack row to see its instrument, effects and sends here.", juce::dontSendNotification);
    hintLabel.setJustificationType (juce::Justification::centred);
    hintLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    hintLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));

    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    KngVolume::setupSlider (volumeSlider);
    volumeSlider.onValueChange = [this] { session.setTrackVolumeDb (trackIndex, (float) volumeSlider.getValue()); };

    panSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange (-1.0, 1.0, 0.01);
    panSlider.setDoubleClickReturnValue (true, 0.0);
    panSlider.onValueChange = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
            if (auto* vp = t->getVolumePlugin())
                vp->setPan ((float) panSlider.getValue());
    };

    instrumentButton.onClick     = [this] { showInstrumentMenu(); };
    openInstrumentButton.onClick = [this] { openInstrument(); };
    addEffectButton.onClick      = [this] { showAddEffectMenu(); };
    newBusButton.onClick         = [this] { newBus(); };

    volumeSlider.setTooltip ("Volume (double-click for 0 dB)");
    panSlider.setTooltip ("Pan (double-click to center)");
    instrumentButton.setTooltip ("Choose or swap this track's instrument");
    openInstrumentButton.setTooltip ("Open the full instrument panel");
    addEffectButton.setTooltip ("Add an effect to this track");
    newBusButton.setTooltip ("Make a new bus (like a Delay Bus) and send this track to it");

    rebuild();
    startTimerHz (5);
}

TrackInspector::~TrackInspector()
{
    stopTimer();
}

//==============================================================================
juce::String TrackInspector::getSignature()
{
    juce::String sig;
    const int idx = session.getSelectedTrackIndex();
    sig << idx << ";";

    if (auto* t = session.getTrack (idx))
    {
        sig << t->itemID.toString() << ";";

        if (session.isMidiTrack (idx))
            if (auto* inst = session.getInstrument (idx))
                sig << "i" << inst->itemID.toString() << ";";

        for (auto* p : session.getFxPlugins (t->pluginList))
            sig << p->itemID.toString() << ",";

        for (int bus : session.getBusNumbers())
            sig << "b" << bus << session.getBusName (bus);
    }

    return sig;
}

void TrackInspector::rebuild()
{
    knobs.clear();
    effectRows.clear();
    sendLabels.clear();
    sendSliders.clear();
    sendBuses.clear();

    trackIndex = session.getSelectedTrackIndex();
    auto* t = session.getTrack (trackIndex);
    const bool has = t != nullptr;

    for (auto* c : std::initializer_list<juce::Component*> { &nameLabel, &instHeading, &fxHeading, &sendHeading,
                                                             &instrumentButton, &openInstrumentButton, &addEffectButton,
                                                             &newBusButton, &volumeSlider, &panSlider })
        c->setVisible (has);

    hintLabel.setVisible (! has);
    lastSignature = getSignature();

    if (! has)
    {
        resized();
        repaint();
        return;
    }

    nameLabel.setText (t->getName(), juce::dontSendNotification);

    const bool midi = session.isMidiTrack (trackIndex);
    const bool drum = midi && session.isDrumTrack (trackIndex);
    const bool bus = session.isReverbBus (trackIndex);
    auto* inst = midi ? session.getInstrument (trackIndex) : nullptr;

    // Instrument card (instrument tracks only)
    instHeading.setVisible (midi);
    instrumentButton.setVisible (midi);
    openInstrumentButton.setVisible (midi && (inst != nullptr || drum));
    instrumentButton.setButtonText (drum ? juce::String ("Drum Kit") : inst != nullptr ? inst->getName() : juce::String ("Choose instrument"));

    if (inst != nullptr && ! drum)
        for (auto* p : pickMainParams (*inst, 4))
            addAndMakeVisible (knobs.add (new ParamKnob (session, p)));

    // Effects
    for (auto* p : session.getFxPlugins (t->pluginList))
    {
        // The instrument has its own card, and the boost stage stays hidden
        if (p == inst || (bool) p->state.getProperty ("kngBoost", false))
            continue;

        addAndMakeVisible (effectRows.add (new EffectRow (session, *p)));
    }

    // Sends (a bus doesn't send to itself)
    sendHeading.setVisible (! bus);
    newBusButton.setVisible (! bus);

    if (! bus)
    {
        const auto buses = session.getBusNumbers();

        if (buses.isEmpty())
            sendBuses.add (-1);
        else
            sendBuses = buses;

        for (int b : sendBuses)
        {
            auto* label = sendLabels.add (new juce::Label());
            label->setText (b < 0 ? juce::String ("Reverb") : session.getBusName (b), juce::dontSendNotification);
            label->setFont (juce::Font (juce::FontOptions (12.0f)));
            addAndMakeVisible (label);

            auto* slider = sendSliders.add (new juce::Slider());
            slider->setSliderStyle (juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
            slider->setRange (-60.0, 0.0, 0.1);
            slider->setDoubleClickReturnValue (true, -60.0);
            slider->textFromValueFunction = [] (double v) { return v <= -60.0 ? juce::String ("Off") : juce::String (v, 1) + " dB"; };
            slider->setValue (b < 0 ? session.getSendDb (trackIndex) : session.getSendToBusDb (trackIndex, b), juce::dontSendNotification);
            slider->onValueChange = [this, slider, b]
            {
                if (b < 0)
                    session.setSendDb (trackIndex, (float) slider->getValue());
                else
                    session.setSendToBusDb (trackIndex, b, (float) slider->getValue());
            };
            addAndMakeVisible (slider);
        }
    }

    if (auto* vp = t->getVolumePlugin())
    {
        volumeSlider.setValue (session.getTrackVolumeDb (trackIndex), juce::dontSendNotification);
        panSlider.setValue (vp->getPan(), juce::dontSendNotification);
    }

    resized();
    repaint();
}

void TrackInspector::timerCallback()
{
    if (getSignature() != lastSignature)
    {
        rebuild();
        return;
    }

    auto* t = session.getTrack (trackIndex);

    if (t == nullptr)
        return;

    if (nameLabel.getText() != t->getName())
        nameLabel.setText (t->getName(), juce::dontSendNotification);

    if (auto* vp = t->getVolumePlugin())
    {
        if (! volumeSlider.isMouseButtonDown())
            volumeSlider.setValue (session.getTrackVolumeDb (trackIndex), juce::dontSendNotification);

        if (! panSlider.isMouseButtonDown())
            panSlider.setValue (vp->getPan(), juce::dontSendNotification);
    }

    for (int i = 0; i < sendSliders.size(); ++i)
    {
        const int b = sendBuses[i];

        if (! sendSliders[i]->isMouseButtonDown())
            sendSliders[i]->setValue (b < 0 ? session.getSendDb (trackIndex) : session.getSendToBusDb (trackIndex, b),
                                      juce::dontSendNotification);
    }
}

//==============================================================================
void TrackInspector::openInstrument()
{
    if (session.isDrumTrack (trackIndex))
    {
        if (session.onShowDrumRack)
            session.onShowDrumRack (trackIndex);

        return;
    }

    auto* inst = session.getInstrument (trackIndex);

    if (dynamic_cast<KngSamplerPlugin*> (inst) != nullptr || dynamic_cast<KngSynthPlugin*> (inst) != nullptr)
    {
        if (session.onShowSampler)
            session.onShowSampler (trackIndex);
    }
    else if (inst != nullptr && inst->getPluginType().startsWith ("kng") && session.onShowPluginEditor)
    {
        session.onShowPluginEditor (*inst);
    }
    else if (inst != nullptr)
    {
        inst->showWindowExplicitly();
    }
}

void TrackInspector::showInstrumentMenu()
{
    auto* s = &session;
    const int idx = trackIndex;
    juce::PopupMenu menu;

    menu.addItem ("KNG Sampler (built-in)", [s, idx] { s->setBuiltInSampler (idx); });
    menu.addItem ("KNG Synth (built-in)", [s, idx] { s->setBuiltInSynth (idx); });
    menu.addItem ("KNG 808 (built-in)", [s, idx] { s->setInstrumentById (idx, "kng808"); });
    menu.addSeparator();

    std::map<juce::String, juce::PopupMenu> byMaker;

    for (auto& d : session.getEdit().engine.getPluginManager().knownPluginList.getTypes())
    {
        if (! d.isInstrument)
            continue;

        const auto maker = d.manufacturerName.isNotEmpty() ? d.manufacturerName : juce::String ("Other");
        byMaker[maker].addItem (d.name, [s, idx, d] { s->setInstrument (idx, d); });
    }

    for (auto& [maker, sub] : byMaker)
        menu.addSubMenu (maker, sub);

    if (session.getInstrument (idx) != nullptr)
    {
        menu.addSeparator();
        menu.addItem ("Remove Instrument", [s, idx] { s->removeInstrument (idx); });
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&instrumentButton));
}

void TrackInspector::showAddEffectMenu()
{
    auto* s = &session;
    const int idx = trackIndex;
    juce::PopupMenu menu;
    std::map<juce::String, juce::PopupMenu> byMaker;

    for (auto& e : PluginBrowser::buildEntries (session.getEdit().engine))
    {
        if (e.header || e.instrument)
            continue;

        const auto id = e.id;
        auto add = [s, idx, id] { s->addEffectById (idx, id); };

        if (e.detail == "KNG")
            menu.addItem (e.name, add);
        else
            byMaker[e.detail.isNotEmpty() ? e.detail : juce::String ("Other")].addItem (e.name, add);
    }

    if (! byMaker.empty())
    {
        menu.addSeparator();

        for (auto& [maker, sub] : byMaker)
            menu.addSubMenu (maker, sub);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addEffectButton));
}

void TrackInspector::newBus()
{
    auto* s = &session;
    const int idx = trackIndex;

    auto* w = new juce::AlertWindow ("New Bus", "Name this bus:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "Delay Bus");
    w->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([s, idx, w] (int result)
    {
        const auto name = w->getTextEditorContents ("name").trim();

        if (result == 1 && name.isNotEmpty())
        {
            const int bus = s->addBus (name);

            if (bus >= 0)
                s->setSendToBusDb (idx, bus, -6.0f);
        }
    }), true);
}

//==============================================================================
void TrackInspector::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff24282c));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    if (! instrumentCard.isEmpty() && instHeading.isVisible())
    {
        g.setColour (juce::Colour (0xff1e2124));
        g.fillRoundedRectangle (instrumentCard.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xff3a8ee6));
        g.drawRoundedRectangle (instrumentCard.toFloat().reduced (0.5f), 8.0f, 1.0f);
    }
}

void TrackInspector::resized()
{
    hintLabel.setBounds (getLocalBounds().reduced (20));

    auto r = getLocalBounds().reduced (12);
    nameLabel.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);

    auto volRow = r.removeFromTop (24);
    panSlider.setBounds (volRow.removeFromRight (70));
    volRow.removeFromRight (6);
    volumeSlider.setBounds (volRow);
    r.removeFromTop (14);

    instrumentCard = {};

    if (instHeading.isVisible())
    {
        instHeading.setBounds (r.removeFromTop (16));
        r.removeFromTop (4);

        instrumentCard = r.removeFromTop (knobs.isEmpty() ? 42 : 112);
        auto c = instrumentCard.reduced (8);
        auto top = c.removeFromTop (26);

        if (openInstrumentButton.isVisible())
        {
            openInstrumentButton.setBounds (top.removeFromRight (60));
            top.removeFromRight (6);
        }

        instrumentButton.setBounds (top);

        if (! knobs.isEmpty())
        {
            c.removeFromTop (8);
            const int w = c.getWidth() / 4;

            for (auto* k : knobs)
                k->setBounds (c.removeFromLeft (w));
        }

        r.removeFromTop (14);
    }

    fxHeading.setBounds (r.removeFromTop (16));
    r.removeFromTop (4);

    for (auto* row : effectRows)
    {
        row->setBounds (r.removeFromTop (32));
        r.removeFromTop (4);
    }

    addEffectButton.setBounds (r.removeFromTop (30));
    r.removeFromTop (14);

    if (sendHeading.isVisible())
    {
        sendHeading.setBounds (r.removeFromTop (16));
        r.removeFromTop (4);

        for (int i = 0; i < sendSliders.size(); ++i)
        {
            auto row = r.removeFromTop (24);
            sendLabels[i]->setBounds (row.removeFromLeft (84));
            sendSliders[i]->setBounds (row);
            r.removeFromTop (4);
        }

        newBusButton.setBounds (r.removeFromTop (26).removeFromLeft (110));
    }
}
