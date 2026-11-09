#include "EditSession.h"
#include "KngSamplerPlugin.h"
#include <algorithm>

namespace te = tracktion;

//==============================================================================
// KNG Sampler: one sample played across the whole keyboard

KngSamplerPlugin* EditSession::getKngSampler (int trackIndex)
{
    if (auto* track = getTrack (trackIndex))
        for (auto* p : track->pluginList.getPlugins())
            if (auto* k = dynamic_cast<KngSamplerPlugin*> (p))
                return k;

    return nullptr;
}

static KngSamplerPlugin* createKngSampler (te::Edit& edit, te::AudioTrack& track)
{
    auto plugin = edit.getPluginCache().createNewPlugin (KngSamplerPlugin::xmlTypeName, juce::PluginDescription());

    if (plugin == nullptr)
        return nullptr;

    // One instrument per track, so swap out the old one (including older samplers)
    for (auto* p : track.pluginList.getPlugins())
        if (p->isSynth() || dynamic_cast<te::SamplerPlugin*> (p) != nullptr)
            p->deleteFromParent();

    track.pluginList.insertPlugin (plugin, 0, nullptr);
    return dynamic_cast<KngSamplerPlugin*> (plugin.get());
}

void EditSession::setBuiltInSampler (int trackIndex)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return;

    beginEdit();
    createKngSampler (*edit, *track);
    save();

    if (onShowSampler)
        onShowSampler (trackIndex);
}

bool EditSession::setSamplerSample (int trackIndex, const juce::File& file, int rootNote)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr || ! file.existsAsFile())
        return false;

    beginEdit();

    auto* k = getKngSampler (trackIndex);

    // Older sampler, or none: upgrade to the KNG engine
    if (k == nullptr)
        k = createKngSampler (*edit, *track);

    if (k == nullptr)
        return false;

    k->state.setProperty (KngSamplerPlugin::rootProp, juce::jlimit (0, 127, rootNote), nullptr);
    const bool ok = k->loadSample (file);

    save();
    return ok;
}

juce::String EditSession::getSamplerSampleName (int trackIndex)
{
    if (auto* k = getKngSampler (trackIndex))
        if (k->getSampleFile() != juce::File())
            return k->getSampleFile().getFileNameWithoutExtension();

    return {};
}

int EditSession::getSamplerRoot (int trackIndex)
{
    if (auto* k = getKngSampler (trackIndex))
        return (int) k->state.getProperty (KngSamplerPlugin::rootProp, 60);

    return 60;
}

void EditSession::setSamplerRoot (int trackIndex, int rootNote)
{
    if (auto* k = getKngSampler (trackIndex))
    {
        k->state.setProperty (KngSamplerPlugin::rootProp, juce::jlimit (0, 127, rootNote), &edit->getUndoManager());
        k->syncFromState();
    }
}

float EditSession::getSamplerGain (int trackIndex)
{
    if (auto* k = getKngSampler (trackIndex))
        return (float) k->state.getProperty (KngSamplerPlugin::gainProp, 0.0f);

    return 0.0f;
}

void EditSession::setSamplerGain (int trackIndex, float gainDb)
{
    if (auto* k = getKngSampler (trackIndex))
    {
        k->state.setProperty (KngSamplerPlugin::gainProp, gainDb, &edit->getUndoManager());
        k->syncFromState();
    }
}

bool EditSession::getSamplerOneShot (int trackIndex)
{
    if (auto* k = getKngSampler (trackIndex))
        return (bool) k->state.getProperty (KngSamplerPlugin::oneShotProp, false);

    return false;
}

void EditSession::setSamplerOneShot (int trackIndex, bool oneShot)
{
    if (auto* k = getKngSampler (trackIndex))
    {
        k->state.setProperty (KngSamplerPlugin::oneShotProp, oneShot, &edit->getUndoManager());
        k->syncFromState();
    }
}

//==============================================================================
// Chop an audio clip onto drum pads

int EditSession::chopClipToPads (te::AudioClipBase& clip, int slices, bool atMarkers)
{
    if (getTransport().isRecording())
        return -1;

    const auto source = clip.getAudioFile().getFile();

    if (! source.existsAsFile())
        return -1;

    const auto pos = clip.getPosition();
    const double clipStart = pos.getStart().inSeconds();
    const double clipLength = pos.getLength().inSeconds();
    const double offset = pos.getOffset().inSeconds();

    // Cut points, in timeline seconds
    std::vector<double> cuts { clipStart };

    if (atMarkers)
    {
        for (auto& m : getMarkers())
        {
            const double t = m->getPosition().getStart().inSeconds();

            if (t > clipStart + 0.01 && t < clipStart + clipLength - 0.01)
                cuts.push_back (t);
        }
    }
    else
    {
        for (int i = 1; i < slices; ++i)
            cuts.push_back (clipStart + clipLength * i / slices);
    }

    cuts.push_back (clipStart + clipLength);
    std::sort (cuts.begin(), cuts.end());

    const int numSlices = juce::jmin (16, (int) cuts.size() - 1);

    if (numSlices < 1)
        return -1;

    addDrumTrack();
    const int index = getNumTracks() - 1;
    auto* track = getTrack (index);
    auto* sampler = getDrumSampler (index);

    if (track == nullptr || sampler == nullptr)
        return -1;

    beginEdit();
    track->setName ("Chops - " + clip.getName());

    // One slice per pad
    for (int i = 0; i < numSlices; ++i)
    {
        const double sliceStart  = offset + (cuts[(size_t) i] - clipStart);
        const double sliceLength = cuts[(size_t) i + 1] - cuts[(size_t) i];

        if (sampler->addSound (source.getFullPathName(), "Slice " + juce::String (i + 1),
                               sliceStart, sliceLength, 0.0f).isNotEmpty())
            continue;

        const int sound = sampler->getNumSounds() - 1;
        const int note = getPadNote (i);
        sampler->setSoundParams (sound, note, note, note);
        sampler->setSoundOpenEnded (sound, true);
    }

    // A pattern that plays the slices in order, so it sounds like the original
    if (auto midi = track->insertMIDIClip (te::TimeRange (pos.getStart(), pos.getEnd()), nullptr))
    {
        midi->setName ("Chops");

        for (int i = 0; i < numSlices; ++i)
        {
            const auto b1 = midi->getContentBeatAtTime (te::TimePosition::fromSeconds (cuts[(size_t) i]));
            const auto b2 = midi->getContentBeatAtTime (te::TimePosition::fromSeconds (cuts[(size_t) i + 1]));
            midi->getSequence().addNote (getPadNote (i), b1, b2 - b1, 100, 0, &edit->getUndoManager());
        }
    }

    // So you don't hear it twice
    clip.setMuted (true);
    save();

    if (onShowDrumRack)
        onShowDrumRack (index);

    return index;
}

//==============================================================================
// Piano roll for a whole track: the clip you're on, or a new one

void EditSession::openPianoRollForTrack (int trackIndex)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr || ! onShowPianoRoll)
        return;

    te::MidiClip* target = nullptr;

    // 1. The MIDI clip you've selected on this track
    if (auto* selected = dynamic_cast<te::MidiClip*> (getSelectedClip()))
        if (selected->getTrack() == track)
            target = selected;

    // 2. The MIDI clip under the playhead
    if (target == nullptr)
    {
        const auto pos = getTransport().getPosition();

        for (auto* c : track->getClips())
            if (auto* mc = dynamic_cast<te::MidiClip*> (c))
                if (pos >= mc->getPosition().getStart() && pos < mc->getPosition().getEnd())
                    target = mc;
    }

    // 3. A new one at the playhead
    if (target == nullptr)
        target = createMidiClip (trackIndex, getTransport().getPosition().inSeconds());

    if (target != nullptr)
        onShowPianoRoll (*target);
}
