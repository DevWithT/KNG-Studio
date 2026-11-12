#include "EditSession.h"
#include "KngSynthPlugin.h"
#include "KngSamplerPlugin.h"

namespace te = tracktion;

static bool findDescription (te::Engine& engine, const juce::String& id, juce::PluginDescription& out)
{
    for (auto& d : engine.getPluginManager().knownPluginList.getTypes())
    {
        if (d.createIdentifierString() == id)
        {
            out = d;
            return true;
        }
    }

    return false;
}

void EditSession::setInstrumentById (int trackIndex, const juce::String& pluginId)
{
    if (pluginId == KngSynthPlugin::xmlTypeName)
    {
        setBuiltInSynth (trackIndex);
        return;
    }

    if (pluginId == KngSamplerPlugin::xmlTypeName)
    {
        setBuiltInSampler (trackIndex);
        return;
    }

    if (pluginId == "kng808")
    {
        setBuiltInInstrument (trackIndex, pluginId);
        return;
    }

    juce::PluginDescription d;

    if (findDescription (engine, pluginId, d))
        setInstrument (trackIndex, d);
}

int EditSession::addInstrumentTrack (const juce::String& pluginId, const juce::String& trackName)
{
    if (getTransport().isRecording())
        return -1;

    addMidiTrack();
    const int index = getNumTracks() - 1;

    if (auto* t = getTrack (index))
        if (trackName.isNotEmpty())
            t->setName (trackName);

    setInstrumentById (index, pluginId);
    return index;
}

void EditSession::addEffectById (int trackIndex, const juce::String& pluginId)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return;

    for (auto& [name, type] : getBuiltInEffects())
    {
        if (type == pluginId)
        {
            addBuiltInPlugin (track->pluginList, pluginId);
            return;
        }
    }

    juce::PluginDescription d;

    if (findDescription (engine, pluginId, d))
        addPlugin (track->pluginList, d);
}
