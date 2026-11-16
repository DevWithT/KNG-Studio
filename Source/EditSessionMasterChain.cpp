#include "EditSession.h"
#include "KngWidthPlugin.h"
#include "KngCompPlugin.h"
#include "KngAirPlugin.h"
#include "KngLimiterPlugin.h"
#include "KngLoudnessPlugin.h"

namespace te = tracktion;

static const juce::Identifier chainTag ("kngMasterChain");

// Sets a knob by its name (used for KNG Comp)
static void setByName (te::Plugin& plugin, const juce::String& name, float value)
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

void EditSession::applyMasterChain (int style)
{
    if (getTransport().isRecording())
        return;

    auto& list = edit->getMasterPluginList();

    // The loudness meter has to exist first, so the chain goes in ahead of it
    getLoudnessMeter();
    beginEdit();

    // Clicking again replaces the chain instead of stacking a second one
    for (int i = list.size(); --i >= 0;)
        if (auto* p = list[i])
            if ((bool) p->state.getProperty (chainTag, false))
                p->deleteFromParent();

    if (style <= 0)
    {
        save();
        return;
    }

    // Ahead of the master volume and the meters, so the meters show the finished sound
    int at = list.size();

    for (int i = 0; i < list.size(); ++i)
    {
        auto* p = list[i];

        if (dynamic_cast<te::VolumeAndPanPlugin*> (p) != nullptr || dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr
             || dynamic_cast<KngLoudnessPlugin*> (p) != nullptr)
        {
            at = i;
            break;
        }
    }

    auto add = [this, &list, &at] (const juce::String& type) -> te::Plugin*
    {
        auto plugin = edit->getPluginCache().createNewPlugin (type, juce::PluginDescription());

        if (plugin == nullptr)
            return nullptr;

        plugin->state.setProperty (chainTag, true, nullptr);
        list.insertPlugin (plugin, at++, nullptr);
        return plugin.get();
    };

    const bool loud = style == 2;

    // 1. Width: keeps the low end in the middle
    if (auto* w = dynamic_cast<KngWidthPlugin*> (add (KngWidthPlugin::xmlTypeName)))
    {
        w->getParam (KngWidthPlugin::monoBass)->setParameter (120.0f, juce::sendNotification);
        w->getParam (KngWidthPlugin::width)->setParameter (1.05f, juce::sendNotification);
    }

    // 2. Comp: gentle glue
    if (auto* c = add (KngCompPlugin::xmlTypeName))
    {
        setByName (*c, "Threshold", loud ? -16.0f : -12.0f);
        setByName (*c, "Ratio", 2.0f);
    }

    // 3. Air: a touch of top end
    if (auto* a = dynamic_cast<KngAirPlugin*> (add (KngAirPlugin::xmlTypeName)))
    {
        a->getParam (KngAirPlugin::air)->setParameter (1.5f, juce::sendNotification);
        a->getParam (KngAirPlugin::presence)->setParameter (0.5f, juce::sendNotification);
        a->getParam (KngAirPlugin::shine)->setParameter (0.1f, juce::sendNotification);
    }

    // 4. Limiter: the loudness, never past -1 dB (streaming services ask for that)
    if (auto* l = dynamic_cast<KngLimiterPlugin*> (add (KngLimiterPlugin::xmlTypeName)))
    {
        l->getParam (KngLimiterPlugin::gain)->setParameter (loud ? 7.0f : 3.5f, juce::sendNotification);
        l->getParam (KngLimiterPlugin::ceiling)->setParameter (-1.0f, juce::sendNotification);
        l->getParam (KngLimiterPlugin::style)->setParameter (loud ? (float) KngLimiterPlugin::punchy
                                                                  : (float) KngLimiterPlugin::transparent, juce::sendNotification);
    }

    save();
}
