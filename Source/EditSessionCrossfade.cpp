#include "EditSession.h"
#include <algorithm>

namespace te = tracktion;

// Where two audio clips on a lane overlap, the first fades out while the second fades in, over the overlap
int EditSession::crossfadeOverlaps (int trackIndex)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr || getTransport().isRecording())
        return 0;

    std::vector<te::WaveAudioClip*> clips;

    for (auto* c : track->getClips())
        if (auto* w = dynamic_cast<te::WaveAudioClip*> (c))
            clips.push_back (w);

    std::sort (clips.begin(), clips.end(), [] (te::WaveAudioClip* a, te::WaveAudioClip* b)
    {
        return a->getPosition().getStart() < b->getPosition().getStart();
    });

    int count = 0;
    bool started = false;

    for (size_t i = 0; i + 1 < clips.size(); ++i)
    {
        auto* a = clips[i];
        const double aEnd = a->getPosition().getEnd().inSeconds();

        for (size_t j = i + 1; j < clips.size(); ++j)
        {
            auto* b = clips[j];
            const double bStart = b->getPosition().getStart().inSeconds();

            if (bStart >= aEnd - 0.001)
                break;

            // Only clips that carry on past the first one's end get a crossfade
            if (b->getPosition().getEnd().inSeconds() <= aEnd)
                continue;

            const double overlap = aEnd - bStart;

            if (! started)
            {
                beginEdit();
                started = true;
            }

            a->setFadeOut (te::TimeDuration::fromSeconds (overlap));
            a->setFadeOutType (te::AudioFadeCurve::convex);
            b->setFadeIn (te::TimeDuration::fromSeconds (overlap));
            b->setFadeInType (te::AudioFadeCurve::convex);
            ++count;
        }
    }

    if (started)
        save();

    return count;
}
