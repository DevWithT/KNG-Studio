#include "TimelineRuler.h"

namespace te = tracktion;

TimelineRuler::TimelineRuler (EditSession& sessionToUse)
    : session (sessionToUse)
{
    startTimerHz (30);
}

TimelineRuler::~TimelineRuler()
{
    stopTimer();
}

double TimelineRuler::xToTime (int x) const
{
    if (getWidth() <= 0)
        return 0.0;

    return juce::jlimit (0.0, viewSeconds, x / (double) getWidth() * viewSeconds);
}

int TimelineRuler::timeToX (double seconds) const
{
    return juce::roundToInt (seconds / viewSeconds * getWidth());
}

double TimelineRuler::snapToBeat (double seconds) const
{
    const double beat = 60.0 / juce::jmax (1.0, session.getBpm());
    return std::round (seconds / beat) * beat;
}

void TimelineRuler::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff222222));

    // Loop region
    const double loopStart = session.getLoopStart();
    const double loopEnd   = session.getLoopEnd();

    if (loopEnd > loopStart)
    {
        const int x1 = timeToX (loopStart);
        const int x2 = timeToX (loopEnd);

        g.setColour (session.isLooping() ? juce::Colour (0xff2e7d32).withAlpha (0.8f)
                                         : juce::Colours::grey.withAlpha (0.35f));
        g.fillRect (x1, 0, juce::jmax (1, x2 - x1), getHeight());
    }

    // Second ticks, labels every 5 seconds
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    for (int s = 0; s <= (int) viewSeconds; ++s)
    {
        const int x = timeToX ((double) s);
        const bool major = (s % 5) == 0;
        const int h = major ? getHeight() / 2 : getHeight() / 4;

        g.setColour (juce::Colours::white.withAlpha (major ? 0.6f : 0.3f));
        g.drawVerticalLine (x, (float) (getHeight() - h), (float) getHeight());

        if (major && s < (int) viewSeconds)
            g.drawText (juce::String::formatted ("%d:%02d", s / 60, s % 60),
                        x + 3, 1, 40, 12, juce::Justification::left);
    }

    // Playhead
    const int px = timeToX (session.getTransport().getPosition().inSeconds());
    g.setColour (juce::Colours::yellow);
    g.fillRect (px, 0, 2, getHeight());
}

void TimelineRuler::mouseDown (const juce::MouseEvent& e)
{
    draggingLoop = false;
    dragStartTime = xToTime (e.x);
}

void TimelineRuler::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4)
        return;

    draggingLoop = true;

    // Snap to beats unless Alt is held
    const bool snap = ! e.mods.isAltDown();
    const double a = snap ? snapToBeat (dragStartTime) : dragStartTime;
    const double b = snap ? snapToBeat (xToTime (e.x)) : xToTime (e.x);

    session.setLoopRange (a, b);
}

void TimelineRuler::mouseUp (const juce::MouseEvent&)
{
    auto& transport = session.getTransport();

    // A plain click (no drag) moves the playhead
    if (! draggingLoop && ! transport.isRecording())
        transport.setPosition (te::TimePosition::fromSeconds (dragStartTime));

    draggingLoop = false;
}
