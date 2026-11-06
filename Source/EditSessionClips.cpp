#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
// Stretch, pitch and gain

double EditSession::getClipSourceBpm (te::AudioClipBase& clip)
{
    const double bpm = clip.getLoopInfo().getBpm (clip.getWaveInfo());
    return bpm > 1.0 ? bpm : getBpm();
}

void EditSession::setClipFollowTempo (te::AudioClipBase& clip, bool follow, double sourceBpm)
{
    beginEdit();

    sourceBpm = juce::jlimit (20.0, 400.0, sourceBpm);

    auto loopInfo = clip.getLoopInfo();
    loopInfo.setBpm (sourceBpm, clip.getWaveInfo());
    clip.setLoopInfo (loopInfo);

    const bool wasFollowing = clip.getAutoTempo();

    if (follow)
        clip.setTimeStretchMode (te::TimeStretcher::soundtouchBetter);

    clip.setAutoTempo (follow);

    // Resize so the clip still covers the same music
    if (follow != wasFollowing)
    {
        const double length = clip.getPosition().getLength().inSeconds();
        const double ratio = follow ? sourceBpm / juce::jmax (1.0, getBpm())
                                    : juce::jmax (1.0, getBpm()) / sourceBpm;

        clip.setLength (te::TimeDuration::fromSeconds (length * ratio), true);
    }

    if (! follow && std::abs (clip.getPitchChange()) < 0.001f)
        clip.setTimeStretchMode (te::TimeStretcher::disabled);

    save();
}

void EditSession::setClipPitch (te::AudioClipBase& clip, float semitones)
{
    // Pitch shifting needs a stretch mode switched on
    if (std::abs (semitones) > 0.001f && clip.getTimeStretchMode() == te::TimeStretcher::disabled)
        clip.setTimeStretchMode (te::TimeStretcher::soundtouchBetter);

    clip.setAutoPitch (false);
    clip.setPitchChange (semitones);

    if (std::abs (semitones) < 0.001f && ! clip.getAutoTempo())
        clip.setTimeStretchMode (te::TimeStretcher::disabled);
}

void EditSession::setClipGain (te::AudioClipBase& clip, float db)
{
    clip.setGainDB (db);
}

//==============================================================================
// Markers

std::vector<te::Clip::Ptr> EditSession::getMarkers()
{
    std::vector<te::Clip::Ptr> result;

    for (auto* m : edit->getMarkerManager().getMarkers())
        result.push_back (m);

    return result;
}

void EditSession::addMarkerAt (double seconds)
{
    beginEdit();
    edit->ensureMarkerTrack();

    auto& mm = edit->getMarkerManager();
    const int id = mm.getNextUniqueID();
    const double beat = 60.0 / juce::jmax (1.0, getBpm());

    auto marker = mm.createMarker (id, te::TimePosition::fromSeconds (juce::jmax (0.0, seconds)),
                                   te::TimeDuration::fromSeconds (beat), nullptr);

    if (marker != nullptr)
        marker->setName ("Marker " + juce::String (id));

    save();
}

void EditSession::jumpToMarker (int direction)
{
    auto& mm = edit->getMarkerManager();
    const double now = getTransport().getPosition().inSeconds();

    te::MarkerClip* marker = direction > 0
        ? mm.getNextMarker (te::TimePosition::fromSeconds (now))
        : mm.getPrevMarker (te::TimePosition::fromSeconds (juce::jmax (0.0, now - 0.05)));

    if (marker != nullptr)
        getTransport().setPosition (marker->getPosition().getStart());
}
