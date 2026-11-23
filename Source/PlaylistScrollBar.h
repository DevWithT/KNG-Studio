#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** The horizontal bar under the Playlist lanes. It covers the song plus one screen of empty bars after it. */
class PlaylistScrollBar : public juce::Component,
                          private juce::ScrollBar::Listener,
                          private juce::Timer
{
public:
    explicit PlaylistScrollBar (EditSession& sessionToUse)
        : session (sessionToUse), bar (false)
    {
        bar.setAutoHide (false);
        bar.addListener (this);
        addAndMakeVisible (bar);
        startTimerHz (20);
        sync();
    }

    ~PlaylistScrollBar() override
    {
        stopTimer();
        bar.removeListener (this);
    }

    void resized() override   { bar.setBounds (getLocalBounds()); }

private:
    void timerCallback() override   { sync(); }

    // Follows the Playlist view, so zooming and scrolling anywhere else moves the bar too
    void sync()
    {
        const double start = session.getViewStart();
        const double length = session.getViewLength();
        const double total = juce::jmax (session.getPlaylistEnd() + length, start + length);

        bar.setRangeLimits (0.0, total, juce::dontSendNotification);
        bar.setCurrentRange (start, length, juce::dontSendNotification);
    }

    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override
    {
        session.setView (newRangeStart, session.getViewLength());
    }

    EditSession& session;
    juce::ScrollBar bar;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaylistScrollBar)
};
