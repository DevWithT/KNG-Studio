#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
void EditSession::addMidiTrack()
{
    addTrack();

    if (auto* t = getTrack (getNumTracks() - 1))
    {
        t->state.setProperty ("kngTrackType", "midi", nullptr);
        t->setName ("MIDI " + juce::String (getNumTracks()));
    }

    syncTrackState();
    save();
}

bool EditSession::isMidiTrack (int index)
{
    if (auto* t = getTrack (index))
        return t->state.getProperty ("kngTrackType").toString() == "midi";

    return false;
}

//==============================================================================
te::MidiClip* EditSession::createMidiClip (int trackIndex, double startSeconds)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr || getTransport().isRecording())
        return nullptr;

    // Start on the bar you clicked in, 4 bars long
    const double bar = 4.0 * 60.0 / juce::jmax (1.0, getBpm());
    const double start = juce::jmax (0.0, std::floor (startSeconds / bar) * bar);

    beginEdit();

    auto clip = track->insertMIDIClip (te::TimeRange (te::TimePosition::fromSeconds (start),
                                                      te::TimePosition::fromSeconds (start + bar * 4.0)),
                                       nullptr);

    if (clip == nullptr)
        return nullptr;

    selectClip (clip.get());
    save();
    return clip.get();
}

//==============================================================================
void EditSession::setInstrument (int trackIndex, const juce::PluginDescription& desc)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return;

    auto plugin = edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, desc);

    if (plugin == nullptr)
        return;

    beginEdit();

    // One instrument per track, so swap out the old one
    for (auto p : track->pluginList.getPlugins())
        if (p->isSynth())
            p->deleteFromParent();

    track->pluginList.insertPlugin (plugin, 0, nullptr);
    plugin->showWindowExplicitly();
    save();
}

te::Plugin* EditSession::getInstrument (int trackIndex)
{
    if (auto* t = getTrack (trackIndex))
        for (auto* p : t->pluginList)
            if (p->isSynth())
                return p;

    return nullptr;
}

void EditSession::removeInstrument (int trackIndex)
{
    if (auto* inst = getInstrument (trackIndex))
    {
        beginEdit();
        inst->deleteFromParent();
        save();
    }
}
