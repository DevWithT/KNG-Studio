#include "DockPanel.h"
#include "PianoRoll.h"
#include "DrumRack.h"
#include "SamplerView.h"
#include "FxChainComponent.h"
#include "CompView.h"
#include "MixerView.h"
#include "RackView.h"

namespace te = tracktion;

static const juce::StringArray& baseTabNames()
{
    static const juce::StringArray names { "Rack", "Piano Roll", "Instrument", "FX", "Comp", "Mixer" };
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
        "The Rack is coming next: every instrument in one place.",
        "Double-click a MIDI clip, or hit Roll on a MIDI track.",
        "Click the instrument button on a MIDI or drum track.",
        "Click FX on a track or on the master strip.",
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

    tabs.setTabBarDepth (28);
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

    if (isDrumTrack)
        slots[instrumentTab]->setContent (createDrumRackPanel (*session, track));
    else
        slots[instrumentTab]->setContent (std::make_unique<SamplerComponent> (*session, track));

    tabs.setTabName (instrumentTab, (isDrumTrack ? "Drums: " : "Sampler: ") + track.getName());
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
}

DockPanel::Tab DockPanel::getCurrentTab() const
{
    return (Tab) tabs.getCurrentTabIndex();
}

//==============================================================================
void DockPanel::timerCallback()
{
    if (session == nullptr)
        return;

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
    tabs.setBounds (getLocalBounds());
}
