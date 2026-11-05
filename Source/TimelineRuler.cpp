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
    return juce::jmax (0.0, session.xToTime (x, getWidth()));
}

int TimelineRuler::timeToX (double seconds) const
{
    return session.timeToX (seconds, getWidth());
}

double TimelineRuler::snapToBeat (double seconds) const
{
    const double beat = 60.0 / juce::jmax (1.0, session.getBpm());
    return std::round (seconds / beat) * beat;
}

void TimelineRuler::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff222222));

    if (getWidth() <= 0)
        return;

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

    // Bars and beats
    const double beat = 60.0 / juce::jmax (1.0, session.getBpm());
    const double pxPerBar = getWidth() / session.getViewLength() * beat * 4.0;

    // Space bar numbers at least 40px apart
    int labelStep = 1;
    while (pxPerBar * labelStep < 40.0 && labelStep < 1024)
        labelStep *= 2;

    const bool showBeats = pxPerBar / 4.0 >= 10.0;
    const double viewEnd = session.getViewStart() + session.getViewLength();

    const int firstBeat = juce::jmax (0, (int) std::floor (session.getViewStart() / beat));
    const int lastBeat  = (int) std::ceil (viewEnd / beat);

    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    for (int n = firstBeat; n <= lastBeat; ++n)
    {
        const bool isBar = (n % 4) == 0;
        const int x = timeToX (n * beat);

        if (isBar)
        {
            const int barIndex = n / 4;
            const bool labelled = (barIndex % labelStep) == 0;

            g.setColour (juce::Colours::white.withAlpha (labelled ? 0.6f : 0.35f));
            g.drawVerticalLine (x, (float) (getHeight() / 2), (float) getHeight());

            if (labelled)
                g.drawText (juce::String (barIndex + 1), x + 3, 1, 40, 12, juce::Justification::left);
        }
        else if (showBeats)
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawVerticalLine (x, (float) (getHeight() * 3 / 4), (float) getHeight());
        }
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

void TimelineRuler::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Ctrl+scroll zooms, anything else scrolls sideways
    if (e.mods.isCommandDown())
    {
        if (wheel.deltaY != 0.0f)
            session.zoomAround (xToTime (e.x), wheel.deltaY > 0.0f ? 0.8 : 1.25);

        return;
    }

    const float d = wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY;
    session.scrollBy (-d * session.getViewLength() * 0.5);
}
