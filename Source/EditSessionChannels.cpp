#include "EditSession.h"
#include "KngSamplerPlugin.h"

namespace te = tracktion;

int EditSession::addSampleChannel (const juce::File& sample)
{
    if (! sample.existsAsFile() || getTransport().isRecording())
        return -1;

    addMidiTrack();
    const int index = getNumTracks() - 1;
    auto* track = getTrack (index);

    if (track == nullptr)
        return -1;

    auto plugin = edit->getPluginCache().createNewPlugin (KngSamplerPlugin::xmlTypeName, juce::PluginDescription());

    if (plugin == nullptr)
        return -1;

    beginEdit();
    track->pluginList.insertPlugin (plugin, 0, nullptr);
    track->setName (sample.getFileNameWithoutExtension());
    track->state.setProperty ("kngChannel", true, nullptr);
    save();

    // Plays at its own pitch on C4, and stops when its note ends
    setSamplerSample (index, sample, 60);
    setSamplerOneShot (index, false);

    return index;
}
