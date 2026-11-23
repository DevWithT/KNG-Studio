#pragma once

#include "EditSession.h"
#include "KngGatePlugin.h"
#include "KngEqPlugin.h"
#include "KngCompPlugin.h"
#include "KngDeEssPlugin.h"
#include "KngAirPlugin.h"

// One-click vocal chain: Gate, EQ, Comp, De-Esser, Air.
// Its plugins are tagged, so clicking again replaces them instead of stacking a second chain.
namespace KngVocalChain
{
    inline const juce::Identifier& tag()
    {
        static const juce::Identifier t ("kngVocalChain");
        return t;
    }

    // Sets a knob by its name (used for KNG Comp and KNG EQ)
    inline void setByName (tracktion::Plugin& plugin, const juce::String& name, float value)
    {
        for (auto* param : plugin.getAutomatableParameters())
        {
            if (param->getParameterName().equalsIgnoreCase (name))
            {
                param->setParameter (value, juce::sendNotification);
                return;
            }
        }
    }

    inline void apply (EditSession& session, int trackIndex)
    {
        auto* track = session.getTrack (trackIndex);

        if (track == nullptr || session.getTransport().isRecording())
            return;

        auto& edit = session.getEdit();
        auto& list = track->pluginList;

        session.beginEdit();

        // Clicking again replaces the chain
        for (int i = list.size(); --i >= 0;)
            if (auto* p = list[i])
                if ((bool) p->state.getProperty (tag(), false))
                    p->deleteFromParent();

        // After the track's other effects, before the volume, meter and sends
        int at = list.size();

        for (int i = 0; i < list.size(); ++i)
        {
            auto* p = list[i];

            if (dynamic_cast<tracktion::VolumeAndPanPlugin*> (p) != nullptr
                 || dynamic_cast<tracktion::LevelMeterPlugin*> (p) != nullptr
                 || dynamic_cast<tracktion::AuxSendPlugin*> (p) != nullptr)
            {
                at = i;
                break;
            }
        }

        auto add = [&edit, &list, &at] (const juce::String& type) -> tracktion::Plugin*
        {
            auto plugin = edit.getPluginCache().createNewPlugin (type, juce::PluginDescription());

            if (plugin == nullptr)
                return nullptr;

            plugin->state.setProperty (tag(), true, nullptr);
            list.insertPlugin (plugin, at++, nullptr);
            return plugin.get();
        };

        // 1. Gate: soft, only quiets the gaps between phrases
        if (auto* g = dynamic_cast<KngGatePlugin*> (add (KngGatePlugin::xmlTypeName)))
        {
            g->getParam (KngGatePlugin::threshold)->setParameter (-50.0f, juce::sendNotification);
            g->getParam (KngGatePlugin::range)->setParameter (18.0f, juce::sendNotification);
            g->getParam (KngGatePlugin::mode)->setParameter (1.0f, juce::sendNotification);
        }

        // 2. EQ: a low cut at 80 Hz clears rumble, everything else is flat for you
        if (auto* e = add (KngEqPlugin::xmlTypeName))
        {
            setByName (*e, "Band 1 Freq", 80.0f);
            setByName (*e, "Band 1 Slope", 1.0f);
            setByName (*e, "Band 1 On", 1.0f);
        }

        // 3. Comp: even out the level
        if (auto* c = add (KngCompPlugin::xmlTypeName))
        {
            setByName (*c, "Threshold", -20.0f);
            setByName (*c, "Ratio", 3.0f);
        }

        // 4. De-Esser: tame the S sounds
        if (auto* d = dynamic_cast<KngDeEssPlugin*> (add (KngDeEssPlugin::xmlTypeName)))
        {
            d->getParam (KngDeEssPlugin::threshold)->setParameter (-26.0f, juce::sendNotification);
            d->getParam (KngDeEssPlugin::range)->setParameter (8.0f, juce::sendNotification);
        }

        // 5. Air: a touch of top end
        if (auto* a = dynamic_cast<KngAirPlugin*> (add (KngAirPlugin::xmlTypeName)))
        {
            a->getParam (KngAirPlugin::air)->setParameter (1.5f, juce::sendNotification);
            a->getParam (KngAirPlugin::presence)->setParameter (0.5f, juce::sendNotification);
            a->getParam (KngAirPlugin::shine)->setParameter (0.1f, juce::sendNotification);
        }

        session.save();
    }
}
