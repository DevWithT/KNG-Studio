#include "EditSession.h"

#include <map>
#include "KngSamplerPlugin.h"

namespace te = tracktion;

//==============================================================================
// Collect and Save: every audio clip and Drum Kit pad sample that lives outside the project folder
// is copied into the project's Audio folder, and the song is pointed at the copy.
// One file used in many places is copied once. Files that no longer exist are counted, not touched.

int EditSession::collectAndSave (int* numMissing)
{
    if (numMissing != nullptr)
        *numMissing = 0;

    if (getTransport().isRecording())
        return -1;

    const auto projectFolder = getProjectFolder();
    std::map<juce::String, juce::File> copies;
    int copied = 0, missing = 0;

    // Returns the file the song should use, or an empty File if nothing needs to change
    auto collect = [&] (const juce::File& source) -> juce::File
    {
        if (source == juce::File() || source.isAChildOf (projectFolder))
            return {};

        if (! source.existsAsFile())
        {
            ++missing;
            return {};
        }

        const auto key = source.getFullPathName();

        if (auto it = copies.find (key); it != copies.end())
            return it->second;

        const auto dest = copyIntoProject (source);

        if (dest == juce::File())
        {
            ++missing;
            return {};
        }

        copies[key] = dest;
        ++copied;
        return dest;
    };

    beginEdit ("Collect and save");

    for (int i = 0; i < getNumTracks(); ++i)
    {
        auto* track = getTrack (i);

        if (track == nullptr)
            continue;

        // Audio clips
        for (auto* c : track->getClips())
        {
            if (auto* wac = dynamic_cast<te::WaveAudioClip*> (c))
            {
                const auto dest = collect (wac->getOriginalFile());

                if (dest != juce::File())
                    wac->getSourceFileReference().setToDirectFileReference (dest, true);
            }
        }

        // KNG Sampler (Rack sound channels and sampler lanes)
        if (auto* sampler = getKngSampler (i))
        {
            const auto dest = collect (sampler->getSampleFile());

            if (dest != juce::File())
            {
                sampler->state.setProperty (KngSamplerPlugin::fileProp, dest.getFullPathName(), &edit->getUndoManager());
                sampler->syncFromState();
            }
        }

        // Drum Kit pads
        if (isDrumTrack (i))
        {
            if (auto* sampler = getDrumSampler (i))
            {
                for (int s = 0; s < sampler->getNumSounds(); ++s)
                {
                    const auto dest = collect (sampler->getSoundFile (s).getFile());

                    if (dest != juce::File())
                        sampler->setSoundMedia (s, dest.getFullPathName());
                }
            }
        }
    }

    save();

    if (numMissing != nullptr)
        *numMissing = missing;

    return copied;
}
