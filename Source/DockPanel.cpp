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
#include "KngTheme.h"

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
    g.fillAll (KngTheme::bg);

    if (content == nullptr)
    {
        g.setColour (KngTheme::muted);
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
        tabs.addTab (baseTabNames()[i], KngTheme::panel2, slot, true);
    }

    // The built-in tab bar stays hidden; the tab row along the top of the dock (drawn in paint) replaces it
    tabs.setTabBarDepth (0);
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
    else if (auto* sampler = track.pluginList.findFirstPluginOfType<KngSamplerPlugin>())
        slots[instrumentTab]->setContent (std::make_unique<KngSamplerView> (*sampler, session));
    else
        slots[instrumentTab]->clear();

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
    // Redraw the tab row so names and the current tab stay in step
    repaint (0, 0, getWidth(), stripHeight);
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

std::vector<std::pair<int, juce::Rectangle<int>>> DockPanel::tabLayout() const
{
    static const int order[] = { rackTab, mixerTab, pianoRollTab, instrumentTab, fxTab, effectTab, compTab };
    const juce::Font font (juce::FontOptions (13.0f, juce::Font::bold));
    const int current = tabs.getCurrentTabIndex();
    std::vector<std::pair<int, juce::Rectangle<int>>> out;
    int x = 6;

    for (int i : order)
    {
        const bool always = i == rackTab || i == mixerTab;

        if (! always && i != current && (slots[i] == nullptr || slots[i]->content == nullptr))
            continue;

        const int w = juce::jmin (240, juce::GlyphArrangement::getStringWidthInt (font, tabs.getTabNames()[i]) + 28);
        out.push_back ({ i, juce::Rectangle<int> (x, 0, w, stripHeight) });
        x += w;
    }

    return out;
}

void DockPanel::paint (juce::Graphics& g)
{
    g.fillAll (KngTheme::panel2);

    // Tab row
    g.setColour (KngTheme::panel);
    g.fillRect (0, 0, getWidth(), stripHeight);
    g.setColour (KngTheme::line);
    g.drawHorizontalLine (stripHeight - 1, 0.0f, (float) getWidth());

    const int current = tabs.getCurrentTabIndex();
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));

    for (auto& t : tabLayout())
    {
        const bool on = t.first == current;

        g.setColour (on ? KngTheme::text : KngTheme::muted);
        g.drawText (tabs.getTabNames()[t.first], t.second.reduced (10, 0), juce::Justification::centredLeft, true);

        if (on)
        {
            g.setColour (KngTheme::accent);
            g.fillRect (t.second.getX() + 4, stripHeight - 3, t.second.getWidth() - 8, 2);
        }
    }
}

void DockPanel::mouseDown (const juce::MouseEvent& e)
{
    if (e.y >= stripHeight)
        return;

    for (auto& t : tabLayout())
    {
        if (t.second.contains (e.getPosition()))
        {
            showTab ((Tab) t.first);
            return;
        }
    }
}

void DockPanel::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (stripHeight);
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
