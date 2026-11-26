#include "EditSession.h"

#include <vector>

namespace te = tracktion;

//==============================================================================
// Chop to Lane: every slice becomes its own audio clip on a new lane, so the slices
// can be dragged around, copied and deleted like any other clip.

int EditSession::chopClipToLane (te::AudioClipBase& clip, const std::vector<double>& cuts)
{
    if (getTransport().isRecording() || cuts.size() < 2)
        return -1;

    const int numSlices = juce::jmin (16, (int) cuts.size() - 1);
    const double clipStartSec = clip.getPosition().getStart().inSeconds();
    const double projectBpm = getBpmAt (clipStartSec);
    const double tempoScale = (sliceSourceBpmForNextChop > 0.0 && projectBpm > 0.0) ? sliceSourceBpmForNextChop / projectBpm : 1.0;

    std::vector<juce::File> files;
    std::vector<double> starts;

    for (int i = 0; i < numSlices; ++i)
    {
        const auto file = writeSliceFile (clip, cuts[(size_t) i], cuts[(size_t) i + 1], i + 1);

        if (! file.existsAsFile())
            continue;

        files.push_back (file);
        starts.push_back (clipStartSec + (cuts[(size_t) i] - clipStartSec) * tempoScale);
    }

    if (files.empty())
        return -1;

    beginEdit ("Chop to lane");
    addTrack();
    const int laneIndex = getNumTracks() - 1;

    if (auto* lane = getTrack (laneIndex))
        lane->setName (clip.getName().isNotEmpty() ? clip.getName() + " slices" : juce::String ("Slices"));

    batchImport = true;

    for (size_t k = 0; k < files.size(); ++k)
        importAudioFileAt (files[k], laneIndex, starts[k]);

    batchImport = false;

    // So you don't hear it twice
    // (still the same undo step as the lane and the clips above)
    clip.setMuted (true);
    save();

    return laneIndex;
}
