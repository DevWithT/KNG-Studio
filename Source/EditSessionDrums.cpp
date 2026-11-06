#include "EditSession.h"
#include <map>

namespace te = tracktion;

//==============================================================================
juce::File EditSession::getKitsFolder()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("KNG Studio")
                      .getChildFile ("Kits");
    folder.createDirectory();
    return folder;
}

void EditSession::addDrumTrack()
{
    addMidiTrack();
    const int index = getNumTracks() - 1;

    if (auto* t = getTrack (index))
    {
        t->state.setProperty ("kngDrum", true, nullptr);
        t->setName ("Drums");

        auto p = edit->getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, juce::PluginDescription());

        if (p != nullptr)
            t->pluginList.insertPlugin (p, 0, nullptr);
    }

    save();
}

bool EditSession::isDrumTrack (int trackIndex)
{
    if (auto* t = getTrack (trackIndex))
        return (bool) t->state.getProperty ("kngDrum", false);

    return false;
}

te::SamplerPlugin* EditSession::getDrumSampler (int trackIndex)
{
    if (auto* t = getTrack (trackIndex))
        return t->pluginList.findFirstPluginOfType<te::SamplerPlugin>();

    return nullptr;
}

//==============================================================================
// Each pad is one sampler sound pinned to its own note (pad 1 = C1, note 36)

static int findPadSound (te::SamplerPlugin& sampler, int pad)
{
    const int note = EditSession::getPadNote (pad);

    for (int i = 0; i < sampler.getNumSounds(); ++i)
        if (sampler.getMinKey (i) == note)
            return i;

    return -1;
}

static bool assignPad (te::Engine& engine, te::SamplerPlugin& sampler, int pad, const juce::File& file)
{
    if (const int existing = findPadSound (sampler, pad); existing >= 0)
        sampler.removeSound (existing);

    te::AudioFile audioFile (engine, file);

    if (! audioFile.isValid())
        return false;

    const auto error = sampler.addSound (file.getFullPathName(), file.getFileNameWithoutExtension(),
                                         0.0, audioFile.getLength(), 0.0f);

    if (error.isNotEmpty())
        return false;

    // One-shot, so hits always ring out fully
    const int index = sampler.getNumSounds() - 1;
    const int note = EditSession::getPadNote (pad);
    sampler.setSoundParams (index, note, note, note);
    sampler.setSoundOpenEnded (index, true);
    return true;
}

juce::String EditSession::getPadName (int trackIndex, int pad)
{
    if (auto* s = getDrumSampler (trackIndex))
        if (const int i = findPadSound (*s, pad); i >= 0)
            return s->getSoundName (i);

    return {};
}

void EditSession::setPadName (int trackIndex, int pad, const juce::String& name)
{
    if (auto* s = getDrumSampler (trackIndex))
    {
        if (const int i = findPadSound (*s, pad); i >= 0)
        {
            beginEdit();
            s->setSoundName (i, name);
            save();
        }
    }
}

bool EditSession::setPadSample (int trackIndex, int pad, const juce::File& file)
{
    auto* s = getDrumSampler (trackIndex);

    if (s == nullptr || ! juce::isPositiveAndBelow (pad, 16))
        return false;

    beginEdit();
    const bool ok = assignPad (engine, *s, pad, file);
    save();
    return ok;
}

void EditSession::clearPad (int trackIndex, int pad)
{
    if (auto* s = getDrumSampler (trackIndex))
    {
        if (const int i = findPadSound (*s, pad); i >= 0)
        {
            beginEdit();
            s->removeSound (i);
            save();
        }
    }
}

float EditSession::getPadGain (int trackIndex, int pad)
{
    if (auto* s = getDrumSampler (trackIndex))
        if (const int i = findPadSound (*s, pad); i >= 0)
            return s->getSoundGainDb (i);

    return 0.0f;
}

float EditSession::getPadPan (int trackIndex, int pad)
{
    if (auto* s = getDrumSampler (trackIndex))
        if (const int i = findPadSound (*s, pad); i >= 0)
            return s->getSoundPan (i);

    return 0.0f;
}

void EditSession::setPadGainAndPan (int trackIndex, int pad, float gainDb, float pan)
{
    if (auto* s = getDrumSampler (trackIndex))
        if (const int i = findPadSound (*s, pad); i >= 0)
            s->setSoundGains (i, gainDb, pan);
}

int EditSession::getPadPitch (int trackIndex, int pad)
{
    // Pitch is the gap between the pad's note and the sample's root note
    if (auto* s = getDrumSampler (trackIndex))
        if (const int i = findPadSound (*s, pad); i >= 0)
            return getPadNote (pad) - s->getKeyNote (i);

    return 0;
}

void EditSession::setPadPitch (int trackIndex, int pad, int semitones)
{
    if (auto* s = getDrumSampler (trackIndex))
    {
        if (const int i = findPadSound (*s, pad); i >= 0)
        {
            const int note = getPadNote (pad);
            s->setSoundParams (i, note - juce::jlimit (-24, 24, semitones), note, note);
        }
    }
}

void EditSession::playPad (int trackIndex, int pad, int velocity)
{
    if (auto* t = getTrack (trackIndex))
        t->playGuideNote (getPadNote (pad), te::MidiChannel (1), velocity, false, true, true);
}

//==============================================================================
// Kit loading: sort samples onto pads by folder and file names

static juce::String categorise (const juce::File& f)
{
    const auto s = (f.getParentDirectory().getFileName() + " " + f.getFileNameWithoutExtension()).toLowerCase();

    auto has = [&s] (std::initializer_list<const char*> words)
    {
        for (auto* w : words)
            if (s.contains (w))
                return true;

        return false;
    };

    const bool isHat = has ({ "hat", "hh", "hihat", "hi-hat" });

    if (has ({ "808", "sub", "bass" }))                                     return "808";
    if (has ({ "kick", "kik" }))                                            return "Kick";
    if (has ({ "snare", "snr" }))                                           return "Snare";
    if (has ({ "clap", "clp" }))                                            return "Clap";
    if (isHat && has ({ "open", "ohh", "_oh", " oh" }))                     return "Open Hat";
    if (isHat)                                                              return "Hat";
    if (has ({ "perc", "shaker", "tamb", "conga", "bongo", "rim", "snap" })) return "Perc";
    if (has ({ "tom" }))                                                    return "Tom";
    if (has ({ "crash", "ride", "cymbal", "cym" }))                         return "Cymbal";

    return "Other";
}

int EditSession::loadKitFolder (int trackIndex, const juce::File& folder)
{
    auto* s = getDrumSampler (trackIndex);

    if (s == nullptr || ! folder.isDirectory())
        return 0;

    // Every audio file, subfolders included
    auto files = folder.findChildFiles (juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac");

    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFullPathName().compareNatural (b.getFullPathName()) < 0;
    });

    std::map<juce::String, juce::Array<juce::File>> byCategory;

    for (auto& f : files)
        byCategory[categorise (f)].add (f);

    // Pad layout: kicks, snares, clap, hats, open hat, 808s, percs, tom, cymbal, extras
    const juce::StringArray slots { "Kick", "Kick", "Snare", "Snare", "Clap", "Hat", "Hat", "Open Hat",
                                    "808", "808", "Perc", "Perc", "Tom", "Cymbal", "Other", "Other" };

    beginEdit();

    while (s->getNumSounds() > 0)
        s->removeSound (0);

    juce::Array<juce::File> chosen;
    chosen.resize (16);

    for (int pad = 0; pad < 16; ++pad)
    {
        auto& queue = byCategory[slots[pad]];

        if (! queue.isEmpty())
        {
            chosen.set (pad, queue.getFirst());
            queue.remove (0);
        }
    }

    // Fill any empty pads with whatever's left
    juce::Array<juce::File> leftovers;

    for (auto& [name, queue] : byCategory)
        leftovers.addArray (queue);

    for (int pad = 0; pad < 16 && ! leftovers.isEmpty(); ++pad)
    {
        if (chosen[pad] == juce::File())
        {
            chosen.set (pad, leftovers.getFirst());
            leftovers.remove (0);
        }
    }

    int filled = 0;

    for (int pad = 0; pad < 16; ++pad)
        if (chosen[pad] != juce::File() && assignPad (engine, *s, pad, chosen[pad]))
            ++filled;

    save();
    return filled;
}
