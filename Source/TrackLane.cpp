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
    clipThumbs.clear();
    recordingThumb = nullptr;
}

int TrackLane::timeToX (double seconds) const
{
    return juce::roundToInt (seconds / viewSeconds * getWidth());
}

void TrackLane::timerCallback()
{
    syncClips();
    syncRecordingThumb();
    repaint();
}

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
            clipThumbs.push_back ({ c, nullptr });
    }

    // Thumbnails can fail right after recording while the file finishes, so retry until ready
    for (auto& ct : clipThumbs)
    {
        if (ct.thumb != nullptr)
            continue;

        if (auto* wac = dynamic_cast<te::WaveAudioClip*> (ct.clip.get()))
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

void TrackLane::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2a2a2a));

    // Gridline every 5 seconds
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (int s = 0; s <= (int) viewSeconds; s += 5)
        g.drawVerticalLine (timeToX ((double) s), 0.0f, (float) getHeight());

    drawClips (g);
    drawRecording (g);

    const int px = timeToX (session.getTransport().getPosition().inSeconds());
    g.setColour (juce::Colours::yellow);
    g.fillRect (px, 0, 2, getHeight());
}

void TrackLane::drawClips (juce::Graphics& g)
{
    for (auto& ct : clipThumbs)
    {
        auto pos = ct.clip->getPosition();
        const bool selected = ct.clip.get() == session.getSelectedClip();

        const int x1 = timeToX (pos.getStart().inSeconds());
        const int x2 = timeToX (pos.getEnd().inSeconds());
        juce::Rectangle<int> area (x1, 0, juce::jmax (1, x2 - x1), getHeight());

        g.setColour (selected ? juce::Colour (0xff4f8fd0) : juce::Colour (0xff3a6ea5));
        g.fillRect (area);

        if (ct.thumb != nullptr)
        {
            const double offset = pos.getOffset().inSeconds();
            const double length = pos.getLength().inSeconds();

            te::TimeRange sourceRange (te::TimePosition::fromSeconds (offset),
                                       te::TimePosition::fromSeconds (offset + length));

            g.setColour (juce::Colours::white.withAlpha (0.85f));
            ct.thumb->drawChannel (g, area.reduced (0, 4), sourceRange, 0, 1.0f);
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

void TrackLane::drawRecording (juce::Graphics& g)
{
    if (recordingThumb == nullptr)
        return;

    const double start  = session.getTransport().getTimeWhenStarted().inSeconds();
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

te::Clip* TrackLane::findClipAt (int x)
{
    for (auto& ct : clipThumbs)
    {
        auto pos = ct.clip->getPosition();
        const int x1 = timeToX (pos.getStart().inSeconds());
        const int x2 = timeToX (pos.getEnd().inSeconds());

        if (x >= x1 && x <= x2)
            return ct.clip.get();
    }

    return nullptr;
}

void TrackLane::mouseDown (const juce::MouseEvent& e)
{
    auto& transport = session.getTransport();

    if (transport.isRecording() || getWidth() <= 0)
        return;

    grabKeyboardFocus();

    auto* clip = findClipAt (e.x);

    if (e.mods.isPopupMenu())
    {
        if (clip != nullptr)
        {
            session.selectClip (clip);

            juce::PopupMenu menu;
            menu.addItem ("Delete Clip", [s = &session] { s->deleteSelectedClip(); });
            menu.showMenuAsync ({});
        }

        return;
    }

    // Clicking empty space clears the selection
    session.selectClip (clip);

    const double secs = juce::jlimit (0.0, viewSeconds, e.x / (double) getWidth() * viewSeconds);
    transport.setPosition (te::TimePosition::fromSeconds (secs));
}

bool TrackLane::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (session.getSelectedClip() != nullptr)
        {
            session.deleteSelectedClip();
            return true;
        }
    }

    return false;
}
