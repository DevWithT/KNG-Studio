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
    folderHeaders.clear();
    autoLanes.clear();
    autoHeaders.clear();
    lanes.clear();
    headers.clear();
}

juce::Array<tracktion::AudioTrack*> TracksView::getCurrentTracks()
{
    juce::Array<tracktion::AudioTrack*> result;

    for (int i = 0; i < session.getNumTracks(); ++i)
        result.add (session.getTrack (i));

    return result;
}

juce::String TracksView::getLayoutSignature()
{
    juce::String sig;

    for (int i = 0; i < session.getNumTracks(); ++i)
    {
        const auto folder = session.getTrackFolder (i);
        sig << folder << "|" << (folder.isNotEmpty() && session.isFolderCollapsed (folder) ? "c" : "o")
            << (session.isAutomationShown (i) ? "a" : "-") << ";";
    }

    return sig;
}

void TracksView::timerCallback()
{
    if (getCurrentTracks() != builtTracks || getLayoutSignature() != lastSignature)
        rebuild();
}

void TracksView::rebuild()
{
    folderHeaders.clear();
    autoLanes.clear();
    autoHeaders.clear();
    lanes.clear();
    headers.clear();

    builtTracks = getCurrentTracks();
    juce::String previousFolder;

    for (int i = 0; i < builtTracks.size(); ++i)
    {
        // One folder bar where each folder begins
        const auto folder = session.getTrackFolder (i);

        if (folder.isNotEmpty() && folder != previousFolder)
            addAndMakeVisible (folderHeaders.add (new FolderHeader (session, folder)));

        previousFolder = folder;

        addAndMakeVisible (headers.add (new TrackHeader (session, i)));
        addAndMakeVisible (lanes.add (new TrackLane (session, i)));
        addChildComponent (autoHeaders.add (new AutomationHeader (session, i)));
        addChildComponent (autoLanes.add (new AutomationLane (session, i)));
    }

    lastSignature = getLayoutSignature();
    updateLayout();
}

int TracksView::layout (bool apply)
{
    int y = 0, folderIndex = 0;
    juce::String previousFolder;
    const int laneWidth = juce::jmax (0, getWidth() - headerWidth - gap);

    for (int i = 0; i < headers.size(); ++i)
    {
        const auto folder = session.getTrackFolder (i);

        if (folder.isNotEmpty() && folder != previousFolder)
        {
            if (apply && folderIndex < folderHeaders.size())
                folderHeaders[folderIndex]->setBounds (0, y, getWidth(), folderHeight);

            ++folderIndex;
            y += folderHeight + gap;
        }

        previousFolder = folder;

        const bool hidden = folder.isNotEmpty() && session.isFolderCollapsed (folder);
        const bool showAuto = ! hidden && session.isAutomationShown (i);
        const int x = folder.isNotEmpty() ? indent : 0;

        if (apply)
        {
            headers[i]->setVisible (! hidden);
            lanes[i]->setVisible (! hidden);
            autoHeaders[i]->setVisible (showAuto);
            autoLanes[i]->setVisible (showAuto);
        }

        if (hidden)
            continue;

        if (apply)
        {
            headers[i]->setBounds (x, y, headerWidth - x, rowHeight);
            lanes[i]->setBounds (headerWidth + gap, y, laneWidth, rowHeight);
        }

        y += rowHeight + gap;

        if (showAuto)
        {
            if (apply)
            {
                autoHeaders[i]->setBounds (x, y, headerWidth - x, autoHeight);
                autoLanes[i]->setBounds (headerWidth + gap, y, laneWidth, autoHeight);
            }

            y += autoHeight + gap;
        }
    }

    if (apply)
    {
        addTrackButton.setBounds (0, y + 4, 140, 28);
        addMidiTrackButton.setBounds (146, y + 4, 170, 28);
    }

    return y + 36;
}

int TracksView::getIdealHeight()
{
    return layout (false);
}

void TracksView::updateLayout()
{
    int h = layout (false);

    if (auto* parent = getParentComponent())
        h = juce::jmax (h, parent->getHeight());

    if (getHeight() != h)
        setSize (getWidth(), h);

    layout (true);
}

void TracksView::resized()
{
    layout (true);
}

//==============================================================================
int TracksView::trackAtY (int y) const
{
    for (int i = 0; i < headers.size(); ++i)
    {
        if (! headers[i]->isVisible())
            continue;

        const int top = headers[i]->getY();
        const int bottom = headers[i]->getBottom() + gap + (autoLanes[i]->isVisible() ? autoHeight + gap : 0);

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
    const int top = addTrackButton.getY() - 4;
    auto area = juce::Rectangle<int> (0, top, getWidth(), juce::jmax (48, getHeight() - top)).toFloat().reduced (2.0f);

    g.setColour (accent.withAlpha (0.1f));
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (accent);
    g.drawRoundedRectangle (area, 6.0f, 2.0f);
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawText ("Drop to create a new track", area.toNearestInt(), juce::Justification::centred);
}
