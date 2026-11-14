#include "MixerView.h"
#include "VolumeHelpers.h"
#include "PluginBrowser.h"
#include "GenericPluginPanel.h"
#include "InspectorCards.h"
#include <map>

namespace te = tracktion;

te::PluginList* MixerStrip::getPluginList()
{
    if (isMaster())
        return &session.getEdit().getMasterPluginList();

    if (auto* t = session.getTrack (trackIndex))
        return &t->pluginList;

    return nullptr;
}

void MixerStrip::rebuildInserts()
{
    auto* list = getPluginList();

    if (list == nullptr)
        return;

    auto fx = session.getFxPlugins (*list);
    juce::String sig;

    for (auto* p : fx)
        sig << p->itemID.toString() << (p->isEnabled() ? "1" : "0") << ";";

    if (sig == lastInsertSignature)
        return;

    lastInsertSignature = sig;
    insertButtons.clear();

    for (auto* p : fx)
    {
        auto* b = insertButtons.add (new InsertButton());
        // Each effect wears its own color (VSTs stay grey), dimmed when bypassed. "KNG " is dropped to save room.
        const auto identity = kngIdentityFor (*p);
        b->setButtonText (p->getName().startsWith ("KNG ") ? p->getName().substring (4) : p->getName());
        b->setColour (juce::TextButton::buttonColourId, p->isEnabled() ? identity.accent.interpolatedWith (juce::Colour (0xff1e2124), 0.55f)
                                                                        : juce::Colour (0xff2b2f33));
        b->setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (p->isEnabled() ? 0.95f : 0.4f));
        b->setTooltip ("Click to open. Right-click to bypass or remove.");

        te::Plugin::Ptr ptr (p);
        juce::Component::SafePointer<MixerStrip> safe (this);

        b->onClick = [safe, ptr, b] { if (safe != nullptr) safe->openPlugin (*ptr, *b); };

        b->onRightClick = [safe, ptr]
        {
            if (safe == nullptr)
                return;

            auto* s = &safe->session;
            juce::PopupMenu menu;
            menu.addItem (ptr->isEnabled() ? "Bypass" : "Turn On", [ptr] { ptr->setEnabled (! ptr->isEnabled()); });
            menu.addItem ("Remove", [s, ptr] { juce::MessageManager::callAsync ([s, ptr] { s->removePlugin (ptr.get()); }); });
            menu.showMenuAsync ({});
        };

        addAndMakeVisible (b);
    }

    resized();
}

void MixerStrip::syncSendBuses()
{
    const auto buses = session.getBusNumbers();
    juce::String sig;

    for (int bus : buses)
        sig << bus << ":" << session.getBusName (bus) << ";";

    if (sig == lastBusSignature)
        return;

    lastBusSignature = sig;
    sendBusBox.clear (juce::dontSendNotification);

    for (int bus : buses)
        sendBusBox.addItem (session.getBusName (bus), bus + 1);

    if (! buses.contains (sendBus) && ! buses.isEmpty())
        sendBus = buses.getFirst();

    sendBusBox.setSelectedId (sendBus + 1, juce::dontSendNotification);
}

void MixerStrip::openPlugin (te::Plugin& plugin, juce::Component& anchor)
{
    if (dynamic_cast<te::ExternalPlugin*> (&plugin) != nullptr)
        plugin.showWindowExplicitly();
    else if (plugin.getPluginType().startsWith ("kng") && session.onShowPluginEditor)
        session.onShowPluginEditor (plugin);
    else
        GenericPluginPanel::showFor (plugin, anchor);
}

void MixerStrip::addEffect (const juce::String& id)
{
    if (! isMaster())
    {
        session.addEffectById (trackIndex, id);
        return;
    }

    // Master: KNG effects by type, VSTs by their description
    auto& masterList = session.getEdit().getMasterPluginList();

    if (id.startsWith ("kng"))
    {
        session.addBuiltInPlugin (masterList, id);
        return;
    }

    for (auto& d : session.getEdit().engine.getPluginManager().knownPluginList.getTypes())
    {
        if (d.createIdentifierString() == id)
        {
            session.addPlugin (masterList, d);
            return;
        }
    }
}

void MixerStrip::showAddFxMenu()
{
    juce::Component::SafePointer<MixerStrip> safe (this);
    juce::PopupMenu menu;
    std::map<juce::String, juce::PopupMenu> byMaker;

    for (auto& e : PluginBrowser::buildEntries (session.getEdit().engine))
    {
        if (e.header || e.instrument)
            continue;

        const auto id = e.id;
        auto add = [safe, id] { if (safe != nullptr) safe->addEffect (id); };

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

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addFxButton));
}
