#include "TracksView.h"
#include "PluginBrowser.h"

TracksView::TracksView (EditSession& sessionToUse)
    : session (sessionToUse)
{
    // The Playlist needs at least one lane
    if (session.getDefaultLaneId().isEmpty())
        session.addTrack();

    // Older songs: patterns stacked on the same bars go into their own lane
    session.separateOverlappingBlocks();

    addAndMakeVisible (addTrackButton);
    addTrackButton.setTooltip ("Add a lane for audio, recording and pattern blocks");

    // Lane height is saved with the song
    rowHeight = juce::jlimit (minRowHeight, maxRowHeight,
                              (int) session.getEdit().state.getProperty ("kngLaneHeight", 64));
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
    blockLayers.clear();
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
            << (session.isAutomationShown (i) ? "a" : "-") << (session.isLaneTrack (i) ? "L" : "x") << ";";
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
    blockLayers.clear();
    folderHeaders.clear();
    autoLanes.clear();
    autoHeaders.clear();
    lanes.clear();
    headers.clear();

    builtTracks = getCurrentTracks();
    juce::String previousFolder;

    for (int i = 0; i < builtTracks.size(); ++i)
    {
        const bool isLane = session.isLaneTrack (i);

        // One folder bar where each folder begins
        const auto folder = session.getTrackFolder (i);

        if (isLane && folder.isNotEmpty() && folder != previousFolder)
            addAndMakeVisible (folderHeaders.add (new FolderHeader (session, folder)));

        if (isLane)
            previousFolder = folder;

        addChildComponent (headers.add (new TrackHeader (session, i)));
        addChildComponent (lanes.add (new TrackLane (session, i)));
        addChildComponent (autoHeaders.add (new AutomationHeader (session, i)));
        addChildComponent (autoLanes.add (new AutomationLane (session, i)));

        // Pattern blocks draw over the lane's audio
        BlockLayer* layer = nullptr;

        if (isLane)
        {
            layer = new BlockLayer (session, i);
            layer->laneAtScreen = [this] (juce::Point<int> screenPos)
            {
                const int t = trackAtY (getLocalPoint (nullptr, screenPos).y);
                return t >= 0 && session.isLaneTrack (t) ? session.getLaneId (t) : juce::String();
            };
            addChildComponent (layer);
        }

        blockLayers.add (layer);
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
        const bool isLane = session.isLaneTrack (i);
        const auto folder = session.getTrackFolder (i);

        if (isLane && folder.isNotEmpty() && folder != previousFolder)
        {
            if (apply && folderIndex < folderHeaders.size())
                folderHeaders[folderIndex]->setBounds (0, y, getWidth(), folderHeight);

            ++folderIndex;
            y += folderHeight + gap;
        }

        if (isLane)
            previousFolder = folder;

        // Instruments live in the Rack and buses in the Mixer, so only lanes show here
        const bool hidden = ! isLane || (folder.isNotEmpty() && session.isFolderCollapsed (folder));
        const bool showAuto = ! hidden && session.isAutomationShown (i);
        const int x = folder.isNotEmpty() ? indent : 0;

        if (apply)
        {
            headers[i]->setVisible (! hidden);
            lanes[i]->setVisible (! hidden);
            autoHeaders[i]->setVisible (showAuto);
            autoLanes[i]->setVisible (showAuto);

            if (auto* layer = blockLayers[i])
                layer->setVisible (! hidden);
        }

        if (hidden)
            continue;

        if (apply)
        {
            headers[i]->setBounds (x, y, headerWidth - x, rowHeight);
            lanes[i]->setBounds (headerWidth + gap, y, laneWidth, rowHeight);

            if (auto* layer = blockLayers[i])
                layer->setBounds (headerWidth + gap, y, laneWidth, rowHeight);
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
        addTrackButton.setBounds (0, y + 4, 140, 28);

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

void TracksView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Ctrl+scroll over the lane headers makes every lane taller or shorter
    if (e.mods.isCommandDown() && e.getEventRelativeTo (this).x < headerWidth)
    {
        if (wheel.deltaY != 0.0f)
            setLaneHeight (rowHeight + (wheel.deltaY > 0.0f ? 8 : -8));

        return;
    }

    juce::Component::mouseWheelMove (e, wheel);
}

void TracksView::setLaneHeight (int newHeight)
{
    newHeight = juce::jlimit (minRowHeight, maxRowHeight, newHeight);

    if (newHeight == rowHeight)
        return;

    rowHeight = newHeight;
    session.getEdit().state.setProperty ("kngLaneHeight", rowHeight, nullptr);
    updateLayout();
    repaint();
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
    return details.description.toString().startsWith ("kngpattern|")
        || PluginBrowser::parseDrag (details.description, e);
}

void TracksView::itemDropped (const SourceDetails& details)
{
    hoverY = -1;
    repaint();

    const auto text = details.description.toString();

    // A pattern from the Patterns list: a block where it was dropped (below the lanes: a new lane)
    if (text.startsWith ("kngpattern|"))
    {
        const int pattern = text.fromFirstOccurrenceOf ("|", false, false).getIntValue();
        int lane = trackAtY (details.localPosition.y);

        if (lane < 0 || ! session.isLaneTrack (lane))
        {
            session.addTrack();
            lane = session.getNumTracks() - 1;
        }

        const int laneWidth = juce::jmax (1, getWidth() - headerWidth - gap);
        const double t = juce::jmax (0.0, session.xToTime (details.localPosition.x - (headerWidth + gap), laneWidth));

        // Lands on the nearest grid line (Snap off or Alt held: right where you let go)
        const double at = juce::ModifierKeys::currentModifiers.isAltDown() ? t : session.snapTime (t);
        session.placePatternInLane (pattern, at, session.getLaneId (lane));
        return;
    }

    PluginBrowser::Entry e;

    if (! PluginBrowser::parseDrag (text, e))
        return;

    const int track = trackAtY (details.localPosition.y);

    // Instruments go in the Rack; effects go on the lane they're dropped on
    if (e.instrument)
        session.addInstrumentTrack (e.id, e.name);
    else if (track >= 0)
        session.addEffectById (track, e.id);
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

    // Below the lanes
    const int top = addTrackButton.getY() - 4;
    auto area = juce::Rectangle<int> (0, top, getWidth(), juce::jmax (48, getHeight() - top)).toFloat().reduced (2.0f);

    g.setColour (accent.withAlpha (0.1f));
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (accent);
    g.drawRoundedRectangle (area, 6.0f, 2.0f);
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawText ("Drop here", area.toNearestInt(), juce::Justification::centred);
}
