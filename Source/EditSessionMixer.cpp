#include "EditSession.h"

namespace te = tracktion;

// Volume, meter, sends and returns are plumbing, not FX the user picked
static bool isUtilityPlugin (te::Plugin* p)
{
    return dynamic_cast<te::VolumeAndPanPlugin*> (p) != nullptr
        || dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr
        || dynamic_cast<te::AuxSendPlugin*> (p) != nullptr
        || dynamic_cast<te::AuxReturnPlugin*> (p) != nullptr;
}

//==============================================================================
juce::Array<te::Plugin*> EditSession::getFxPlugins (te::PluginList& list)
{
    juce::Array<te::Plugin*> result;

    for (auto* p : list)
        if (! isUtilityPlugin (p))
            result.add (p);

    return result;
}

void EditSession::addPlugin (te::PluginList& list, const juce::PluginDescription& desc)
{
    auto plugin = edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, desc);

    if (plugin == nullptr)
        return;

    beginEdit();

    // FX go before the volume, meter and sends, so they're pre-fader
    int index = list.size();

    for (int i = 0; i < list.size(); ++i)
    {
        auto* p = list[i];

        if (dynamic_cast<te::VolumeAndPanPlugin*> (p) != nullptr
            || dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr
            || dynamic_cast<te::AuxSendPlugin*> (p) != nullptr)
        {
            index = i;
            break;
        }
    }

    list.insertPlugin (plugin, index, nullptr);
    plugin->showWindowExplicitly();
}

void EditSession::movePlugin (te::PluginList& list, te::Plugin* plugin, int direction)
{
    auto fx = getFxPlugins (list);
    const int i = fx.indexOf (plugin);
    const int j = i + direction;

    if (i < 0 || ! juce::isPositiveAndBelow (j, fx.size()))
        return;

    const int from = list.state.indexOf (plugin->state);
    const int to   = list.state.indexOf (fx[j]->state);

    if (from < 0 || to < 0)
        return;

    beginEdit();
    list.state.moveChild (from, to, &edit->getUndoManager());
}

void EditSession::removePlugin (te::Plugin* plugin)
{
    if (plugin == nullptr)
        return;

    beginEdit();
    plugin->deleteFromParent();
}

//==============================================================================
te::AuxSendPlugin* EditSession::findSend (te::AudioTrack& track)
{
    return track.pluginList.findFirstPluginOfType<te::AuxSendPlugin>();
}

bool EditSession::isReverbBus (int trackIndex)
{
    if (auto* t = getTrack (trackIndex))
        return t->pluginList.findFirstPluginOfType<te::AuxReturnPlugin>() != nullptr;

    return false;
}

float EditSession::getSendDb (int trackIndex)
{
    if (auto* t = getTrack (trackIndex))
        if (auto* send = findSend (*t))
            return send->getGainDb();

    return -60.0f;
}

void EditSession::setSendDb (int trackIndex, float db)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr || isReverbBus (trackIndex))
        return;

    auto* send = findSend (*track);

    if (send == nullptr)
    {
        if (db <= -60.0f)
            return;

        ensureReverbBus();

        auto p = edit->getPluginCache().createNewPlugin (te::AuxSendPlugin::create());

        if (auto* s = dynamic_cast<te::AuxSendPlugin*> (p.get()))
        {
            s->busNumber = 0;

            // Post-fader: goes after the volume and meter
            track->pluginList.insertPlugin (p, track->pluginList.size(), nullptr);
            send = s;
        }
    }

    if (send != nullptr)
        send->setGainDb (db);
}

void EditSession::ensureReverbBus()
{
    for (int i = 0; i < getNumTracks(); ++i)
        if (isReverbBus (i))
            return;

    addTrack();
    auto* bus = getTrack (getNumTracks() - 1);

    if (bus == nullptr)
        return;

    bus->setName ("Reverb Bus");

    auto p = edit->getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, juce::PluginDescription());

    if (auto* r = dynamic_cast<te::AuxReturnPlugin*> (p.get()))
    {
        r->busNumber = 0;
        bus->pluginList.insertPlugin (p, 0, nullptr);
    }
}

//==============================================================================
te::Plugin* EditSession::getMasterMeterPlugin()
{
    auto& list = edit->getMasterPluginList();

    if (auto* m = list.findFirstPluginOfType<te::LevelMeterPlugin>())
        return m;

    auto p = edit->getPluginCache().createNewPlugin (te::LevelMeterPlugin::xmlTypeName, juce::PluginDescription());

    if (p != nullptr)
        list.insertPlugin (p, list.size(), nullptr);

    return list.findFirstPluginOfType<te::LevelMeterPlugin>();
}
