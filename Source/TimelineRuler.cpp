#include "TimelineRuler.h"

namespace te = tracktion;

TimelineRuler::TimelineRuler (EditSession& sessionToUse)
    : session (sessionToUse)
{
    setTooltip ("Click to move the playhead, drag to set the loop. Right-click to add a tempo or time signature change.");
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
    return session.snapToBeat (seconds);
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

        g.setColour (session.isPunchEnabled() ? juce::Colour (0xffb03030).withAlpha (0.8f)
                   : session.isLooping()      ? juce::Colour (0xff2e7d32).withAlpha (0.8f)
                                              : juce::Colours::grey.withAlpha (0.35f));
        g.fillRect (x1, 0, juce::jmax (1, x2 - x1), getHeight());
    }

    // Bars and beats, following the tempo map
    auto& ts = session.getEdit().tempoSequence;
    const double viewStart = session.getViewStart();
    const double viewEnd = viewStart + session.getViewLength();

    double bar = session.barStartAt (viewStart);
    int barNumber = session.barNumberAt (viewStart);
    const double pxPerBar = timeToX (session.barsAfter (bar, 1)) - timeToX (bar);

    int labelStep = 1;

    while (pxPerBar * labelStep < 40.0 && labelStep < 1024)
        labelStep *= 2;

    const bool showBeats = pxPerBar / 4.0 >= 10.0;
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    for (int guard = 0; bar <= viewEnd && guard < 4000; ++guard)
    {
        const double next = session.barsAfter (bar, 1);

        if (next <= bar)
            break;

        const int x = timeToX (bar);
        const bool labelled = ((barNumber - 1) % labelStep) == 0;

        g.setColour (juce::Colours::white.withAlpha (labelled ? 0.6f : 0.35f));
        g.drawVerticalLine (x, (float) (getHeight() / 2), (float) getHeight());

        if (labelled)
            g.drawText (juce::String (barNumber), x + 3, 1, 40, 12, juce::Justification::left);

        if (showBeats)
        {
            const double b1 = ts.toBeats (te::TimePosition::fromSeconds (bar)).inBeats();
            const double b2 = ts.toBeats (te::TimePosition::fromSeconds (next)).inBeats();
            g.setColour (juce::Colours::white.withAlpha (0.25f));

            for (double b = std::floor (b1) + 1.0; b < b2 - 1.0e-6; b += 1.0)
                g.drawVerticalLine (timeToX (ts.toTime (te::BeatPosition::fromBeats (b)).inSeconds()),
                                    (float) (getHeight() * 3 / 4), (float) getHeight());
        }

        bar = next;
        ++barNumber;
    }

    // Tempo (orange) and time signature (purple) changes
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));

    for (auto& m : session.getTempoMarks())
    {
        const int x = timeToX (m.seconds);

        if (x < -60 || x > getWidth())
            continue;

        const auto colour = m.isTimeSig ? juce::Colour (0xff8e6ae6) : juce::Colour (0xffe6893a);
        g.setColour (colour);
        g.fillRect (x, 0, 2, getHeight());
        g.fillRoundedRectangle ((float) x, (float) getHeight() - 11.0f, 52.0f, 11.0f, 2.0f);
        g.setColour (juce::Colours::black);
        g.drawText (m.label, x + 3, getHeight() - 11, 50, 11, juce::Justification::centredLeft);
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

    if (! e.mods.isPopupMenu())
        return;

    // Right-click: tempo and time signature changes
    dragStartTime = -1.0;

    auto* s = &session;
    const double t = xToTime (e.x);
    const int bar = s->barNumberAt (t);
    juce::PopupMenu menu;

    menu.addItem ("Add Tempo Change at Bar " + juce::String (bar) + "...", [s, t]
    {
        auto* w = new juce::AlertWindow ("Tempo Change", "New tempo from this bar (BPM):", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("bpm", juce::String (juce::roundToInt (s->getBpmAt (t))));
        w->addButton ("Add", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        w->enterModalState (true, juce::ModalCallbackFunction::create ([s, t, w] (int result)
        {
            const double bpm = w->getTextEditorContents ("bpm").getDoubleValue();

            if (result == 1 && bpm > 0.0)
                s->addTempoChange (t, bpm);
        }), true);
    });

    juce::PopupMenu sigs;

    for (auto* sig : { "4/4", "3/4", "6/8", "5/4", "7/8", "12/8", "2/4" })
    {
        sigs.addItem (sig, [s, t, sig]
        {
            const auto parts = juce::StringArray::fromTokens (sig, "/", "");
            s->addTimeSigChange (t, parts[0].getIntValue(), parts[1].getIntValue());
        });
    }

    menu.addSubMenu ("Time Signature at Bar " + juce::String (bar), sigs);

    // Remove a change near where you clicked
    for (auto& m : s->getTempoMarks())
    {
        if (std::abs (timeToX (m.seconds) - e.x) <= 10)
        {
            const auto mark = m;
            menu.addSeparator();
            menu.addItem ("Remove " + m.label + " change", [s, mark] { s->removeTempoMark (mark); });
        }
    }

    menu.showMenuAsync ({});
}

void TimelineRuler::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStartTime < 0.0 || e.getDistanceFromDragStart() < 4)
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
    if (dragStartTime >= 0.0 && ! draggingLoop && ! transport.isRecording())
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
