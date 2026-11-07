#include "EditSession.h"
#include <algorithm>

namespace te = tracktion;

//==============================================================================
// Built-in sampler: one sample played across the whole keyboard

void EditSession::setBuiltInSampler (int trackIndex)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return;

    auto plugin = edit->getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, juce::PluginDescription());

    if (plugin == nullptr)
        return;

    beginEdit();

    // One instrument per track, so swap out the old one
    for (auto p : track->pluginList.getPlugins())
        if (p->isSynth() || dynamic_cast<te::SamplerPlugin*> (p.get()) != nullptr)
            p->deleteFromParent();

    track->pluginList.insertPlugin (plugin, 0, nullptr);
    save();

    if (onShowSampler)
        onShowSampler (trackIndex);
}

bool EditSession::setSamplerSample (int trackIndex, const juce::File& file, int rootNote)
{
    auto* s = getDrumSampler (trackIndex);

    if (s == nullptr)
        return false;

    te::AudioFile audioFile (engine, file);

    if (! audioFile.isValid())
        return false;

    beginEdit();

    // One sampler per track: remove any extra left over from earlier swaps
    if (auto* track = getTrack (trackIndex))
        for (auto p : track->pluginList.getPlugins())
            if (auto* extra = dynamic_cast<te::SamplerPlugin*> (p.get()))
                if (extra != s)
                    extra->deleteFromParent();

    while (s->getNumSounds() > 0)
        s->removeSound (0);

    const auto error = s->addSound (file.getFullPathName(), file.getFileNameWithoutExtension(),
                                    0.0, audioFile.getLength(), 0.0f);

    if (error.isNotEmpty())
        return false;

    // Play it across every key, pitched from the root note
    s->setSoundParams (0, juce::jlimit (0, 127, rootNote), 0, 127);
    s->setSoundOpenEnded (0, false);

    save();
    return true;
}

juce::String EditSession::getSamplerSampleName (int trackIndex)
{
    if (auto* s = getDrumSampler (trackIndex))
        if (s->getNumSounds() > 0)
            return s->getSoundName (0);

    return {};
}

int EditSession::getSamplerRoot (int trackIndex)
{
    if (auto* s = getDrumSampler (trackIndex))
        if (s->getNumSounds() > 0)
            return s->getKeyNote (0);

    return 60;
}

void EditSession::setSamplerRoot (int trackIndex, int rootNote)
{
    if (auto* s = getDrumSampler (trackIndex))
        if (s->getNumSounds() > 0)
            s->setSoundParams (0, juce::jlimit (0, 127, rootNote), 0, 127);
}

float EditSession::getSamplerGain (int trackIndex)
{
    if (auto* s = getDrumSampler (trackIndex))
        if (s->getNumSounds() > 0)
            return s->getSoundGainDb (0);

    return 0.0f;
}

void EditSession::setSamplerGain (int trackIndex, float gainDb)
{
    if (auto* s = getDrumSampler (trackIndex))
        if (s->getNumSounds() > 0)
            s->setSoundGains (0, gainDb, s->getSoundPan (0));
}

bool EditSession::getSamplerOneShot (int trackIndex)
{
    if (auto* s = getDrumSampler (trackIndex))
        if (s->getNumSounds() > 0)
            return s->isSoundOpenEnded (0);

    return false;
}

void EditSession::setSamplerOneShot (int trackIndex, bool oneShot)
{
    if (auto* s = getDrumSampler (trackIndex))
        if (s->getNumSounds() > 0)
            s->setSoundOpenEnded (0, oneShot);
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
