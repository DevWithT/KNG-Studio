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

    addAndMakeVisible (addMidiTrackButton);
    addMidiTrackButton.onClick = [this]
    {
        session.addMidiTrack();
        rebuild();
    };

    rebuild();
    addDrumTrackButton.onClick = [this]
    {
        session.addDrumTrack();

        if (session.onShowDrumRack)
            session.onShowDrumRack (session.getNumTracks() - 1);

        rebuild();
    };
    startTimerHz (5);
}

TracksView::~TracksView()
{
    stopTimer();
    autoLanes.clear();
    autoHeaders.clear();
    lanes.clear();
    headers.clear();
}

juce::String TracksView::getAutomationSignature() const
{
    juce::String sig;

    for (int i = 0; i < headers.size(); ++i)
        sig << (session.isAutomationShown (i) ? "1" : "0");

    return sig;
}

int TracksView::getIdealHeight() const
{
    int h = 0;

    for (int i = 0; i < headers.size(); ++i)
    {
        h += rowHeight + gap;

        if (session.isAutomationShown (i))
            h += autoHeight + gap;
    }

    return h + 36;
}

juce::Array<tracktion::AudioTrack*> TracksView::getCurrentTracks()
{
    juce::Array<tracktion::AudioTrack*> result;

    for (int i = 0; i < session.getNumTracks(); ++i)
        result.add (session.getTrack (i));

    return result;
}

void TracksView::timerCallback()
{
    if (getCurrentTracks() != builtTracks)
    {
        rebuild();
        return;
    }

    if (getAutomationSignature() != lastAutoSignature)
        updateLayout();
}

void TracksView::rebuild()
{
    autoLanes.clear();
    autoHeaders.clear();
    lanes.clear();
    headers.clear();

    builtTracks = getCurrentTracks();

    for (int i = 0; i < builtTracks.size(); ++i)
    {
        addAndMakeVisible (headers.add (new TrackHeader (session, i)));
        addAndMakeVisible (lanes.add (new TrackLane (session, i)));
        addChildComponent (autoHeaders.add (new AutomationHeader (session, i)));
        addChildComponent (autoLanes.add (new AutomationLane (session, i)));
    }

    updateLayout();
}

void TracksView::updateLayout()
{
    lastAutoSignature = getAutomationSignature();
    setSize (getWidth(), getIdealHeight());
    resized();
}

void TracksView::resized()
{
    int y = 0;
    const int laneWidth = juce::jmax (0, getWidth() - headerWidth - gap);

    for (int i = 0; i < headers.size(); ++i)
    {
        headers[i]->setBounds (0, y, headerWidth, rowHeight);
        lanes[i]->setBounds (headerWidth + gap, y, laneWidth, rowHeight);
        y += rowHeight + gap;

        const bool shown = session.isAutomationShown (i);
        autoHeaders[i]->setVisible (shown);
        autoLanes[i]->setVisible (shown);

        if (shown)
        {
            autoHeaders[i]->setBounds (0, y, headerWidth, autoHeight);
            autoLanes[i]->setBounds (headerWidth + gap, y, laneWidth, autoHeight);
            y += autoHeight + gap;
        }
    }

    addTrackButton.setBounds (0, y + 4, 140, 28);
    addMidiTrackButton.setBounds (146, y + 4, 170, 28);
}
