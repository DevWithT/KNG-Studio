#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
// Built-in effects

std::vector<std::pair<juce::String, juce::String>> EditSession::getBuiltInEffects()
{
    return {
        { "EQ (4 band)",          te::EqualiserPlugin::xmlTypeName },
        { "Compressor",           te::CompressorPlugin::xmlTypeName },
        { "Reverb",               te::ReverbPlugin::xmlTypeName },
        { "Delay",                te::DelayPlugin::xmlTypeName },
        { "Chorus",               te::ChorusPlugin::xmlTypeName },
        { "Phaser",               te::PhaserPlugin::xmlTypeName },
        { "Low / High Pass",      te::LowPassPlugin::xmlTypeName },
        { "Pitch Shift",          te::PitchShiftPlugin::xmlTypeName }
    };
}

void EditSession::addBuiltInPlugin (te::PluginList& list, const juce::String& type)
{
    auto plugin = edit->getPluginCache().createNewPlugin (type, juce::PluginDescription());

    if (plugin == nullptr)
        return;

    beginEdit();

    // Effects go before the track's own volume and meter
    int index = list.size();

    for (int i = 0; i < list.size(); ++i)
    {
        auto* p = list[i];

        if (dynamic_cast<te::VolumeAndPanPlugin*> (p) != nullptr
             || dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr)
        {
            index = i;
            break;
        }
    }

    list.insertPlugin (plugin, index, nullptr);
    save();
}

//==============================================================================
// Bounce in place

void EditSession::bounceTrackInPlace (int trackIndex)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr || getTransport().isRecording() || ! trackHasContent (trackIndex))
        return;

    const auto name = track->getName();
    auto folder = getProjectFolder().getChildFile ("Audio");
    folder.createDirectory();

    const auto dest = folder.getNonexistentChildFile (juce::File::createLegalFileName (name + " bounce"), ".wav");
    te::Track::Ptr source (track);

    bounceHandle = exportStem (trackIndex, dest, false, [this, dest, name, source] (const juce::String&)
    {
        if (! dest.existsAsFile())
            return;

        // The bounce starts at 0, the same as the stem export
        addTrack();
        const int newIndex = getNumTracks() - 1;
        importAudioFileAt (dest, newIndex, 0.0);

        if (auto* bounced = getTrack (newIndex))
            bounced->setName (name + " (bounced)");

        if (auto* original = dynamic_cast<te::AudioTrack*> (source.get()))
            original->setMute (true);

        save();
    });
}

//==============================================================================
// Move a pattern copy

void EditSession::movePatternBlock (int pattern, double fromSeconds, double toSeconds)
{
    if (getTransport().isRecording())
        return;

    toSeconds = juce::jmax (0.0, toSeconds);
    beginEdit();

    for (int i = 0; i < getNumTracks(); ++i)
    {
        auto* track = getTrack (i);

        if (track == nullptr)
            continue;

        juce::Array<te::Clip*> toMove;

        for (auto* c : track->getClips())
            if ((int) c->state.getProperty ("kngPattern", 0) == pattern
                 && std::abs (c->getPosition().getStart().inSeconds() - fromSeconds) < 0.001)
                toMove.add (c);

        for (auto* c : toMove)
            c->setStart (te::TimePosition::fromSeconds (toSeconds), false, true);
    }

    // If that was the pattern's home copy, its home moves with it
    const juce::Identifier homeKey ("kngPatternHome" + juce::String (pattern));

    if (edit->state.hasProperty (homeKey) && std::abs ((double) edit->state.getProperty (homeKey) - fromSeconds) < 0.001)
        edit->state.setProperty (homeKey, toSeconds, nullptr);

    save();
}
