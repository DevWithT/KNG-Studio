#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
void EditSession::save()
{
    // Remember this project's zoom
    edit->state.setProperty ("kngViewStart", viewStart, nullptr);
    edit->state.setProperty ("kngViewLength", viewLength, nullptr);

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

//==============================================================================
juce::File EditSession::copyIntoProject (const juce::File& file)
{
    if (file.isAChildOf (getProjectFolder()))
        return file;

    auto audioFolder = getProjectFolder().getChildFile ("Audio");
    audioFolder.createDirectory();

    auto dest = audioFolder.getNonexistentChildFile (file.getFileNameWithoutExtension(),
                                                     file.getFileExtension(), false);

    return file.copyFileTo (dest) ? dest : juce::File();
}

double EditSession::importAudioFileAt (const juce::File& file, int trackIndex, double startSeconds)
{
    if (getTransport().isRecording())
        return -1.0;

    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return -1.0;

    auto dest = copyIntoProject (file);

    if (dest == juce::File())
        return -1.0;

    te::AudioFile audioFile (engine, dest);

    if (! audioFile.isValid())
        return -1.0;

    const double start = juce::jmax (0.0, startSeconds);
    const double end   = start + audioFile.getLength();

    beginEdit();

    const te::TimeRange range (te::TimePosition::fromSeconds (start), te::TimePosition::fromSeconds (end));
    auto clip = track->insertWaveClip (file.getFileNameWithoutExtension(), dest,
                                       te::ClipPosition { range, te::TimeDuration() }, false);

    if (clip == nullptr)
        return -1.0;

    save();
    return end;
}
