#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
void EditSession::save()
{
    te::EditFileOperations (*edit).save (true, true, false);
}

//==============================================================================
bool EditSession::importAudioFile (const juce::File& file)
{
    if (getTransport().isRecording())
        return false;

    // Copy the file into the project's Audio folder so the project owns it
    auto dest = file;

    if (! file.isAChildOf (getProjectFolder()))
    {
        auto audioFolder = getProjectFolder().getChildFile ("Audio");
        audioFolder.createDirectory();

        dest = audioFolder.getNonexistentChildFile (file.getFileNameWithoutExtension(),
                                                    file.getFileExtension(), false);

        if (! file.copyFileTo (dest))
            return false;
    }

    te::AudioFile audioFile (engine, dest);

    if (! audioFile.isValid())
        return false;

    addTrack();
    auto* track = getTrack (getNumTracks() - 1);

    if (track == nullptr)
        return false;

    track->setName (file.getFileNameWithoutExtension());

    const te::TimeRange range (te::TimePosition(),
                               te::TimePosition::fromSeconds (audioFile.getLength()));

    auto clip = track->insertWaveClip (file.getFileNameWithoutExtension(), dest,
                                       te::ClipPosition { range, te::TimeDuration() }, false);

    save();
    return clip != nullptr;
}
