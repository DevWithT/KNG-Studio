#include "DockPanel.h"
#include "PianoRoll.h"
#include "DrumRack.h"
#include "FxChainComponent.h"
#include "CompView.h"
#include "MixerView.h"
#include "RackView.h"
#include "KngSamplerView.h"
#include "KngSynthView.h"
#include "KngSynthPlugin.h"

namespace te = tracktion;

static const juce::StringArray& baseTabNames()
{
    static const juce::StringArray names { "Rack", "Piano Roll", "Instrument", "FX", "Effect", "Comp", "Mixer" };
    return names;
}

//==============================================================================
void DockPanel::Slot::setContent (std::unique_ptr<juce::Component> newContent)
{
    content = std::move (newContent);

    if (content != nullptr)
    {
        addAndMakeVisible (*content);
        content->setBounds (getLocalBounds());
    }

    repaint();
}

void DockPanel::Slot::clear()
{
    content = nullptr;
    repaint();
}

void DockPanel::Slot::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));

    if (content == nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText (message, getLocalBounds(), juce::Justification::centred);
    }
}

void DockPanel::Slot::resized()
{
    if (content != nullptr)
        content->setBounds (getLocalBounds());
}

//==============================================================================
DockPanel::DockPanel()
{
    const juce::StringArray messages {
        "Open a project to see the Rack.",
        "Double-click a MIDI clip, or click a melody row in the Rack.",
        "Click Open on the instrument in the inspector on the right.",
        "Click FX on the master strip.",
        "Click an effect in the inspector on the right.",
        "Right-click a clip with takes > Comp Takes.",
        "Open a project to see the mixer."
    };

    for (int i = 0; i < numTabs; ++i)
    {
        auto* slot = new Slot();
        slot->message = messages[i];
        slots[i] = slot;
        tabs.addTab (baseTabNames()[i], juce::Colour (0xff24282c), slot, true);
    }

    // No tab row: the Rack and Mixer buttons, the inspector and clicks on clips decide what shows here
    tabs.setTabBarDepth (0);

    addAndMakeVisible (titleLabel);
    titleLabel.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    tabs.setOutline (0);
    addAndMakeVisible (tabs);

    startTimerHz (2);
}

DockPanel::~DockPanel()
{
    stopTimer();
    setSession (nullptr);
}

juce::String DockPanel::getTrackName (te::Track* track)
{
    return track != nullptr ? track->getName() : juce::String();
}

void DockPanel::resetTabName (Tab tab)
{
    tabs.setTabName ((int) tab, baseTabNames()[(int) tab]);
}

void DockPanel::setSession (EditSession* sessionToUse)
{
    for (auto* slot : slots)
        if (slot != nullptr)
            slot->clear();

    pianoClip = nullptr;
    instrumentTrack = nullptr;
    fxTrack = nullptr;
    compClip = nullptr;
    effectPlugin = nullptr;

    for (int i = 0; i < numTabs; ++i)
        resetTabName ((Tab) i);

    session = sessionToUse;

    if (session != nullptr)
    {
        slots[rackTab]->setContent (std::make_unique<RackView> (*session));
        slots[mixerTab]->setContent (std::make_unique<MixerView> (*session));
        showTab (rackTab);
    }
}

//==============================================================================
void DockPanel::showPianoRoll (te::MidiClip& clip)
{
    if (session == nullptr)
        return;

    pianoClip = &clip;
    slots[pianoRollTab]->setContent (std::make_unique<PianoRoll> (*session, clip));
    tabs.setTabName (pianoRollTab, "Piano Roll: " + getTrackName (clip.getTrack()));
    showTab (pianoRollTab);
}

void DockPanel::showInstrument (te::Track& track, bool isDrumTrack)
{
    if (session == nullptr)
        return;

    instrumentTrack = &track;

    const bool isSynth = ! isDrumTrack && track.pluginList.findFirstPluginOfType<KngSynthPlugin>() != nullptr;

    if (isDrumTrack)
        slots[instrumentTab]->setContent (createDrumRackPanel (*session, track));
    else if (isSynth)
        slots[instrumentTab]->setContent (std::make_unique<KngSynthView> (*track.pluginList.findFirstPluginOfType<KngSynthPlugin>()));
    else
        slots[instrumentTab]->setContent (std::make_unique<KngSamplerView> (*session, track));

    tabs.setTabName (instrumentTab, (isDrumTrack ? "Drum Kit - " : isSynth ? "Synth - " : "Sampler - ") + track.getName());
    showTab (instrumentTab);
}

void DockPanel::showFx (te::PluginList& list, const juce::String& title)
{
    if (session == nullptr)
        return;

    fxTrack = list.getOwnerTrack();
    slots[fxTab]->setContent (std::make_unique<FxChainComponent> (*session, list));
    tabs.setTabName (fxTab, title);
    showTab (fxTab);
}

void DockPanel::showComp (te::WaveAudioClip& clip)
{
    if (session == nullptr)
        return;

    compClip = &clip;
    slots[compTab]->setContent (std::make_unique<CompView> (*session, clip));
    tabs.setTabName (compTab, "Comp: " + getTrackName (clip.getTrack()));
    showTab (compTab);
}

void DockPanel::showTab (Tab tab)
{
    tabs.setCurrentTabIndex ((int) tab);
    updateTitle();
}

void DockPanel::updateTitle()
{
    const auto name = tabs.getTabNames()[tabs.getCurrentTabIndex()];

    if (titleLabel.getText() != name)
        titleLabel.setText (name, juce::dontSendNotification);
}

DockPanel::Tab DockPanel::getCurrentTab() const
{
    return (Tab) tabs.getCurrentTabIndex();
}

//==============================================================================
void DockPanel::timerCallback()
{
    updateTitle();

    if (session == nullptr)
        return;

    if (effectPlugin != nullptr && effectPlugin->getOwnerList() == nullptr)
    {
        slots[effectTab]->clear();
        effectPlugin = nullptr;
        resetTabName (effectTab);
    }

    // Clear any tab whose clip or track was deleted
    if (pianoClip != nullptr && pianoClip->getParent() == nullptr)
    {
        slots[pianoRollTab]->clear();
        pianoClip = nullptr;
        resetTabName (pianoRollTab);
    }

    if (compClip != nullptr && compClip->getParent() == nullptr)
    {
        slots[compTab]->clear();
        compClip = nullptr;
        resetTabName (compTab);
    }

    const auto tracks = te::getAllTracks (session->getEdit());

    if (instrumentTrack != nullptr && ! tracks.contains (instrumentTrack.get()))
    {
        slots[instrumentTab]->clear();
        instrumentTrack = nullptr;
        resetTabName (instrumentTab);
    }

    if (fxTrack != nullptr && ! tracks.contains (fxTrack.get()))
    {
        slots[fxTab]->clear();
        fxTrack = nullptr;
        resetTabName (fxTab);
    }
}

void DockPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));
}

void DockPanel::resized()
{
    auto r = getLocalBounds();
    titleLabel.setBounds (r.removeFromTop (28).reduced (10, 0));
    tabs.setBounds (r);
}

void DockPanel::showEffect (te::Plugin& plugin)
{
    if (session == nullptr)
        return;

    effectPlugin = &plugin;
    slots[effectTab]->setContent (plugin.createEditor());
    tabs.setTabName (effectTab, plugin.getName());
    showTab (effectTab);
}
