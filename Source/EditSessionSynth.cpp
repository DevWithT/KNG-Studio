#include "EditSession.h"
#include "KngSynthPlugin.h"

namespace te = tracktion;

void EditSession::setBuiltInSynth (int trackIndex)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return;

    auto plugin = edit->getPluginCache().createNewPlugin (KngSynthPlugin::xmlTypeName, juce::PluginDescription());

    if (plugin == nullptr)
        return;

    beginEdit();

    // One instrument per track, so swap out the old one
    for (auto* p : track->pluginList.getPlugins())
        if (p->isSynth() || dynamic_cast<te::SamplerPlugin*> (p) != nullptr)
            p->deleteFromParent();

    track->pluginList.insertPlugin (plugin, 0, nullptr);
    save();

    // Opens in the dock's Instrument tab, the same way the sampler does
    if (onShowSampler)
        onShowSampler (trackIndex);
}
