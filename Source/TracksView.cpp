#include "TracksView.h"

TracksView::TracksView (EditSession& sessionToUse)
    : session (sessionToUse)
{
    addAndMakeVisible (addTrackButton);
    addTrackButton.onClick = [this]
    {
        session.addTrack();
        rebuild();
    };

    rebuild();
    startTimerHz (5);
}

TracksView::~TracksView()
{
    stopTimer();
    lanes.clear();
    headers.clear();
}

int TracksView::getIdealHeight() const
{
    return headers.size() * (rowHeight + gap) + 36;
}

void TracksView::timerCallback()
{
    if (session.getNumTracks() != builtCount)
        rebuild();
}

void TracksView::rebuild()
{
    lanes.clear();
    headers.clear();

    builtCount = session.getNumTracks();

    for (int i = 0; i < builtCount; ++i)
    {
        auto* h = headers.add (new TrackHeader (session, i));
        addAndMakeVisible (h);

        auto* l = lanes.add (new TrackLane (session, i));
        addAndMakeVisible (l);
    }

    setSize (getWidth(), getIdealHeight());
    resized();
}

void TracksView::resized()
{
    int y = 0;

    for (int i = 0; i < headers.size(); ++i)
    {
        headers[i]->setBounds (0, y, headerWidth, rowHeight);
        lanes[i]->setBounds (headerWidth + gap, y, juce::jmax (0, getWidth() - headerWidth - gap), rowHeight);
        y += rowHeight + gap;
    }

    addTrackButton.setBounds (0, y + 4, 140, 28);
}
