#include "TrackLane.h"

namespace te = tracktion;

TrackLane::TrackLane (EditSession& sessionToUse, int trackIndexToUse)
    : session (sessionToUse), trackIndex (trackIndexToUse)
{
    setWantsKeyboardFocus (true);
    startTimerHz (30);
}

TrackLane::~TrackLane()
{
    stopTimer();
    drag.clip = nullptr;
    clipThumbs.clear();
    recordingThumb = nullptr;
}

int TrackLane::timeToX (double seconds) const
{
    return session.timeToX (seconds, getWidth());
}

double TrackLane::xToTime (int x) const
{
    return session.xToTime (x, getWidth());
}

void TrackLane::timerCallback()
{
    syncClips();
    syncRecordingThumb();
    repaint();
}

//==============================================================================
void TrackLane::syncClips()
{
    juce::Array<te::Clip*> current;

    if (auto* track = session.getTrack (trackIndex))
        for (auto* c : track->getClips())
            if (dynamic_cast<te::WaveAudioClip*> (c) != nullptr)
                current.add (c);

    bool changed = (int) clipThumbs.size() != current.size();

    for (size_t i = 0; ! changed && i < clipThumbs.size(); ++i)
        changed = clipThumbs[i].clip.get() != current[(int) i];

    if (changed)
    {
        clipThumbs.clear();

        for (auto* c : current)
            clipThumbs.push_back ({ c, nullptr, -1 });
    }

    for (auto& ct : clipThumbs)
    {
        auto* wac = dynamic_cast<te::WaveAudioClip*> (ct.clip.get());

        if (wac == nullptr)
            continue;

        // A different take plays a different file, so rebuild the thumbnail
        const int currentTake = wac->getCurrentTake();

        if (ct.take != currentTake)
        {
            ct.take = currentTake;
            ct.thumb = nullptr;
        }

        // Thumbnails can fail right after recording while the file finishes, so retry until ready
        if (ct.thumb == nullptr)
        {
            te::AudioFile af (wac->getAudioFile());

            if (af.isValid())
                ct.thumb = std::make_unique<te::SmartThumbnail> (wac->edit.engine,
                                                                 te::AudioFile (wac->getPlaybackFile()),
                                                                 *this, &wac->edit);
        }
    }
}

void TrackLane::syncRecordingThumb()
{
    auto& edit = session.getEdit();

    if (! edit.getTransport().isRecording())
    {
        recordingThumb = nullptr;
        return;
    }

    if (recordingThumb != nullptr)
        return;

    if (auto* track = session.getTrack (trackIndex))
    {
        for (auto idi : edit.getEditInputDevices().getDevicesForTargetTrack (*track))
        {
            auto file = idi->getRecordingFile (track->itemID);

            if (file.exists())
            {
                recordingThumb = edit.engine.getRecordingThumbnailManager().getThumbnailFor (file);
                break;
            }
        }
    }
}

//==============================================================================
void TrackLane::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2a2a2a));

    // Loop region tint
    if (session.isLooping() && session.getLoopEnd() > session.getLoopStart())
    {
        const int lx1 = timeToX (session.getLoopStart());
        const int lx2 = timeToX (session.getLoopEnd());
        g.setColour (juce::Colour (0xff2e7d32).withAlpha (0.12f));
        g.fillRect (lx1, 0, juce::jmax (1, lx2 - lx1), getHeight());
    }

    drawGrid (g);
    drawClips (g);
    drawRecording (g);

    const int px = timeToX (session.getTransport().getPosition().inSeconds());
    g.setColour (juce::Colours::yellow);
    g.fillRect (px, 0, 2, getHeight());
}

void TrackLane::drawGrid (juce::Graphics& g)
{
    if (getWidth() <= 0)
        return;

    const double beat = 60.0 / juce::jmax (1.0, session.getBpm());
    const double pxPerBeat = getWidth() / session.getViewLength() * beat;

    // Too zoomed out to draw anything useful
    if (pxPerBeat * 4.0 < 4.0)
        return;

    const bool showBeats = pxPerBeat >= 12.0;
    const double viewEnd = session.getViewStart() + session.getViewLength();

    const int firstBeat = (int) std::floor (session.getViewStart() / beat);
    const int lastBeat  = (int) std::ceil (viewEnd / beat);

    for (int n = juce::jmax (0, firstBeat); n <= lastBeat; ++n)
    {
        const bool isBar = (n % 4) == 0;

        if (! isBar && ! showBeats)
            continue;

        g.setColour (juce::Colours::white.withAlpha (isBar ? 0.10f : 0.04f));
        g.drawVerticalLine (timeToX (n * beat), 0.0f, (float) getHeight());
    }
}

void TrackLane::drawClips (juce::Graphics& g)
{
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    const double viewStart = session.getViewStart();
    const double viewEnd   = viewStart + session.getViewLength();

    for (auto& ct : clipThumbs)
    {
        auto pos = ct.clip->getPosition();
        const bool selected = ct.clip.get() == session.getSelectedClip();

        const double clipStart = pos.getStart().inSeconds();
        const double clipEnd   = pos.getEnd().inSeconds();

        // Skip clips fully off screen
        if (clipEnd < viewStart || clipStart > viewEnd)
            continue;

        const int x1 = timeToX (clipStart);
        const int x2 = timeToX (clipEnd);
        juce::Rectangle<int> area (x1, 0, juce::jmax (1, x2 - x1), getHeight());

        g.setColour (selected ? juce::Colour (0xff4f8fd0) : juce::Colour (0xff3a6ea5));
        g.fillRect (area);

        // Only draw the visible part of the waveform
        if (ct.thumb != nullptr)
        {
            const double offset   = pos.getOffset().inSeconds();
            const double visStart = juce::jmax (clipStart, viewStart);
            const double visEnd   = juce::jmin (clipEnd, viewEnd);

            if (visEnd > visStart)
            {
                const int vx1 = timeToX (visStart);
                const int vx2 = timeToX (visEnd);
                juce::Rectangle<int> visArea (vx1, 4, juce::jmax (1, vx2 - vx1), juce::jmax (1, getHeight() - 8));

                te::TimeRange sourceRange (te::TimePosition::fromSeconds (offset + (visStart - clipStart)),
                                           te::TimePosition::fromSeconds (offset + (visEnd - clipStart)));

                g.setColour (juce::Colours::white.withAlpha (0.85f));
                ct.thumb->drawChannel (g, visArea, sourceRange, 0, 1.0f);
            }
        }

        drawFades (g, *ct.clip, area);

        // Take label
        if (auto* wac = dynamic_cast<te::WaveAudioClip*> (ct.clip.get()))
        {
            const int numTakes = wac->getNumTakes (false);

            if (numTakes > 1)
            {
                g.setColour (juce::Colours::white);
                g.drawText ("Take " + juce::String (wac->getCurrentTake() + 1) + "/" + juce::String (numTakes),
                            area.reduced (12, 2).removeFromTop (14), juce::Justification::left);
            }
        }

        if (selected)
        {
            g.setColour (juce::Colours::white);
            g.drawRect (area, 2);
        }
        else
        {
            g.setColour (juce::Colours::black);
            g.drawRect (area);
        }
    }
}

void TrackLane::drawFades (juce::Graphics& g, te::Clip& clip, juce::Rectangle<int> area)
{
    auto* acb = dynamic_cast<te::AudioClipBase*> (&clip);

    if (acb == nullptr)
        return;

    auto pos = clip.getPosition();
    const double fadeIn  = acb->getFadeIn().inSeconds();
    const double fadeOut = acb->getFadeOut().inSeconds();

    const int xIn  = timeToX (pos.getStart().inSeconds() + fadeIn);
    const int xOut = timeToX (pos.getEnd().inSeconds() - fadeOut);

    const float top = (float) area.getY();
    const float bottom = (float) area.getBottom();

    // Darken the faded corners
    g.setColour (juce::Colours::black.withAlpha (0.35f));

    if (fadeIn > 0.0)
    {
        juce::Path p;
        p.startNewSubPath ((float) area.getX(), top);
        p.lineTo ((float) xIn, top);
        p.lineTo ((float) area.getX(), bottom);
        p.closeSubPath();
        g.fillPath (p);
    }

    if (fadeOut > 0.0)
    {
        juce::Path p;
        p.startNewSubPath ((float) xOut, top);
        p.lineTo ((float) area.getRight(), top);
        p.lineTo ((float) area.getRight(), bottom);
        p.closeSubPath();
        g.fillPath (p);
    }

    // Fade lines
    g.setColour (juce::Colours::white.withAlpha (0.7f));

    if (fadeIn > 0.0)
        g.drawLine ((float) area.getX(), bottom, (float) xIn, top, 1.5f);

    if (fadeOut > 0.0)
        g.drawLine ((float) xOut, top, (float) area.getRight(), bottom, 1.5f);

    // Handles
    g.setColour (juce::Colours::white);
    g.fillRect (xIn - fadeHandleSize / 2, area.getY(), fadeHandleSize, fadeHandleSize);
    g.fillRect (xOut - fadeHandleSize / 2, area.getY(), fadeHandleSize, fadeHandleSize);
}

void TrackLane::drawRecording (juce::Graphics& g)
{
    if (recordingThumb == nullptr)
        return;

    auto& transport = session.getTransport();

    // While loop recording, just mark the loop region
    if (session.isLooping() && session.getLoopEnd() > session.getLoopStart())
    {
        const int x1 = timeToX (session.getLoopStart());
        const int x2 = timeToX (session.getLoopEnd());
        g.setColour (juce::Colours::red.withAlpha (0.35f));
        g.fillRect (x1, 0, juce::jmax (1, x2 - x1), getHeight());
        return;
    }

    const double start  = transport.getTimeWhenStarted().inSeconds();
    const double length = recordingThumb->thumb->getTotalLength();

    const int x1 = timeToX (start);
    const int x2 = timeToX (start + length);

    if (x2 <= x1)
        return;

    juce::Rectangle<int> area (x1, 0, x2 - x1, getHeight());

    g.setColour (juce::Colours::red.withAlpha (0.35f));
    g.fillRect (area);

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    recordingThumb->thumb->drawChannels (g, area.reduced (0, 4), 0.0, length, 1.0f);
}
