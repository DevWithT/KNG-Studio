#include "EditSession.h"

namespace te = tracktion;

void EditSession::setBuiltInInstrument (int trackIndex, const juce::String& type)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr || getTransport().isRecording())
        return;

    auto plugin = edit->getPluginCache().createNewPlugin (type, juce::PluginDescription());

    if (plugin == nullptr)
        return;

    beginEdit();

    // One instrument per track: the old one goes
    for (int i = track->pluginList.size(); --i >= 0;)
        if (auto* p = track->pluginList[i])
            if (p->isSynth())
                p->deleteFromParent();

    track->pluginList.insertPlugin (plugin, 0, nullptr);
    save();
}
