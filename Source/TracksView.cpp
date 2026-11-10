#include "TracksView.h"
#include "PluginBrowser.h"

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
    addMidiTrackButton.setTooltip ("Pick an instrument and get a new track with it, ready to play");
    addMidiTrackButton.onClick = [this]
    {
        PluginBrowser::showInstrumentMenu (session.getEdit().engine, addMidiTrackButton,
                                           [this] (const PluginBrowser::Entry& e) { session.addInstrumentTrack (e.id, e.name); });
    };

    rebuild();
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

//==============================================================================
int TracksView::trackAtY (int y) const
{
    for (int i = 0; i < headers.size(); ++i)
    {
        const int top = headers[i]->getY();
        const int bottom = i + 1 < headers.size() ? headers[i + 1]->getY() : headers[i]->getBottom() + gap;

        if (y >= top && y < bottom)
            return i;
    }

    return -1;
}

bool TracksView::isInterestedInDragSource (const SourceDetails& details)
{
    PluginBrowser::Entry e;
    return PluginBrowser::parseDrag (details.description, e);
}

void TracksView::itemDropped (const SourceDetails& details)
{
    hoverY = -1;
    repaint();

    PluginBrowser::Entry e;

    if (! PluginBrowser::parseDrag (details.description, e))
        return;

    const int track = trackAtY (details.localPosition.y);

    if (e.instrument)
    {
        // On an instrument track: swap its instrument. Anywhere else: a new track.
        if (track >= 0 && session.isMidiTrack (track) && ! session.isDrumTrack (track))
            session.setInstrumentById (track, e.id);
        else
            session.addInstrumentTrack (e.id, e.name);
    }
    else if (track >= 0)
    {
        session.addEffectById (track, e.id);
    }
}

void TracksView::paintOverChildren (juce::Graphics& g)
{
    if (hoverY < 0)
        return;

    const juce::Colour accent (0xff3a8ee6);
    const int track = trackAtY (hoverY);

    if (track >= 0)
    {
        auto row = juce::Rectangle<int> (0, headers[track]->getY(), getWidth(), rowHeight).toFloat().reduced (1.0f);
        g.setColour (accent.withAlpha (0.12f));
        g.fillRoundedRectangle (row, 5.0f);
        g.setColour (accent);
        g.drawRoundedRectangle (row, 5.0f, 2.0f);
        return;
    }

    // Below the tracks: a new track goes here
    const int top = headers.isEmpty() ? 0 : headers.getLast()->getBottom() + gap;
    auto area = juce::Rectangle<int> (0, top, getWidth(), juce::jmax (48, getHeight() - top)).toFloat().reduced (2.0f);

    g.setColour (accent.withAlpha (0.1f));
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (accent);
    g.drawRoundedRectangle (area, 6.0f, 2.0f);
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawText ("Drop to create a new track", area.toNearestInt(), juce::Justification::centred);
}
