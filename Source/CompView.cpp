#include "CompView.h"
#include "CleanTakes.h"

namespace te = tracktion;

//==============================================================================
CompView::CompView (EditSession& sessionToUse, te::WaveAudioClip& clipToComp)
    : session (sessionToUse), clip (&clipToComp)
{
    addAndMakeVisible (flattenButton);
    flattenButton.onClick = [this] { flatten(); };

    addAndMakeVisible (cleanButton);
    cleanButton.setTooltip ("Delete the takes this clip is not using");
    cleanButton.onClick = [this] { cleanTakes(); };

    addAndMakeVisible (hintLabel);
    hintLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    hintLabel.setText ("Drag across a take to use it there. Click to swap a section. Click a name for the whole take. Triangle listens.",
                       juce::dontSendNotification);

    timerCallback();
    startTimerHz (10);

    setSize (960, toolbarHeight + 8 + (numTakes + 1) * (rowHeight + rowGap));
}

CompView::~CompView()
{
    stopTimer();
    stopAudition();
    thumbs.clear();
}

te::WaveCompManager& CompView::cm()
{
    return clip->getCompManager();
}

juce::Colour CompView::colourForTake (int take)
{
    static const juce::uint32 palette[] = { 0xff3a8ee6, 0xffe6893a, 0xff2e8b57, 0xffc2185b,
                                            0xff8e44ad, 0xffc9a227, 0xff16a085, 0xffc0392b };

    if (take < 0)
        return juce::Colour (0xff444444);

    return juce::Colour (palette[take % 8]);
}

//==============================================================================
void CompView::timerCallback()
{
    // Clip was deleted; the dock clears this panel
    if (clip->getParent() == nullptr)
        return;

    // Listening ends when playback stops or passes the end of the clip
    if (auditioning >= 0)
    {
        auto& transport = session.getTransport();

        if (! transport.isPlaying() || transport.getPosition().inSeconds() >= clip->getPosition().getEnd().inSeconds())
        {
            if (transport.isPlaying())
                session.stop();

            stopAudition();
        }
    }

    auto& m = cm();
    numTakes = m.getNumTakes();
    range = m.getCompRange();

    if (range.isEmpty())
        range = { 0.0, juce::jmax (1.0, m.getMaxCompLength()) };

    m.updateThumbnails (*this, thumbs);
    repaint();
}

std::vector<CompView::Section> CompView::getSections()
{
    std::vector<Section> result;
    auto& m = cm();

    if (! m.isCurrentTakeComp())
    {
        result.push_back ({ range.getStart(), range.getEnd(), m.getActiveTakeIndex() });
        return result;
    }

    const int active = m.getActiveTakeIndex();

    for (int s = 0; s < 1024; ++s)
    {
        auto section = m.getSection (active, s);

        if (! section.isValid())
            break;

        auto times = m.getSectionTimes (section);
        result.push_back ({ times.getStart(), times.getEnd(), (int) section.getProperty (te::IDs::takeIndex, -1) });
    }

    return result;
}

//==============================================================================
void CompView::ensureComp()
{
    auto& m = cm();

    if (m.isCurrentTakeComp())
        return;

    // Start the comp as the take you were already on, across the whole clip
    const int previousTake = m.getActiveTakeIndex();

    m.addNewComp();
    m.setActiveTakeIndex (m.getTotalNumTakes() - 1);
    m.changeSectionIndexAtTime (range.getStart() + range.getLength() * 0.5, previousTake);
}

void CompView::useTakeForRange (int takeIndex, double t1, double t2)
{
    session.beginEdit();
    ensureComp();

    auto& m = cm();

    t1 = juce::jlimit (range.getStart(), range.getEnd(), t1);
    t2 = juce::jlimit (range.getStart(), range.getEnd(), t2);

    if (t2 - t1 < 0.01)
        return;

    // Cut the comp at both edges, then point every section in between at this take
    if (t1 > range.getStart() + 0.001)
        m.splitSectionAtTime (t1);

    if (t2 < range.getEnd() - 0.001)
        m.splitSectionAtTime (t2);

    double t = t1 + 0.0005;

    for (int guard = 0; t < t2 && guard < 512; ++guard)
    {
        m.changeSectionIndexAtTime (t, takeIndex);

        auto times = m.getSectionTimes (m.findSectionAtTime (t));

        if (times.getEnd() <= t)
            break;

        t = times.getEnd() + 0.0005;
    }

    m.triggerCompRender();
    session.save();
    repaint();
}

void CompView::useTakeAt (int takeIndex, double t)
{
    session.beginEdit();
    ensureComp();

    cm().changeSectionIndexAtTime (t, takeIndex);
    cm().triggerCompRender();

    session.save();
    repaint();
}

void CompView::flatten()
{
    stopAudition();

    auto& m = cm();

    if (! m.isCurrentTakeComp())
        return;

    session.beginEdit();
    m.flattenTake (m.getActiveTakeIndex(), false);
    session.save();

}

//==============================================================================
void CompView::cleanTakes()
{
    stopAudition();

    juce::Component::SafePointer<CompView> safe (this);
    KngCleanTakes::run (session, *clip, [safe]
    {
        if (safe != nullptr)
            safe->repaint();
    });
}

void CompView::startAudition (int row)
{
    stopAudition();

    auto& m = cm();

    // One take on its own: switch the clip to that take until listening ends
    if (row >= 1)
    {
        auditionRestoreTake = m.getActiveTakeIndex();
        m.setActiveTakeIndex (row - 1);
    }

    auditioning = row;
    session.getTransport().setPosition (clip->getPosition().getStart());
    session.play();
}

void CompView::stopAudition()
{
    if (auditioning < 0)
        return;

    auditioning = -1;

    // Back to the comp
    if (auditionRestoreTake >= 0 && clip->getParent() != nullptr)
        cm().setActiveTakeIndex (auditionRestoreTake);

    auditionRestoreTake = -1;
    repaint();
}

void CompView::drawPlayButton (juce::Graphics& g, juce::Rectangle<int> labelArea, bool on) const
{
    auto box = labelArea.removeFromRight (playZone).withSizeKeepingCentre (14, 14).toFloat();

    g.setColour (on ? juce::Colours::white : juce::Colours::white.withAlpha (0.75f));

    if (on)
    {
        g.fillRect (box.reduced (2.0f));
    }
    else
    {
        juce::Path p;
        p.addTriangle (box.getX() + 2.0f, box.getY(), box.getX() + 2.0f, box.getBottom(),
                       box.getRight(), box.getCentreY());
        g.fillPath (p);
    }
}

juce::Rectangle<int> CompView::getRowArea (int row) const
{
    return { 6, toolbarHeight + 4 + row * (rowHeight + rowGap), getWidth() - 12, rowHeight };
}

int CompView::rowAt (int y) const
{
    const int rel = y - (toolbarHeight + 4);

    if (rel < 0)
        return -1;

    const int row = rel / (rowHeight + rowGap);
    return row <= numTakes ? row : -1;
}

int CompView::timeToX (double t) const
{
    const int left = 6 + labelWidth;
    const int width = juce::jmax (1, getWidth() - 12 - labelWidth);
    return left + juce::roundToInt ((t - range.getStart()) / juce::jmax (1.0e-6, range.getLength()) * width);
}

double CompView::xToTime (int x) const
{
    const int left = 6 + labelWidth;
    const int width = juce::jmax (1, getWidth() - 12 - labelWidth);
    return range.getStart() + (x - left) / (double) width * range.getLength();
}

//==============================================================================
void CompView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));

    g.setColour (juce::Colour (0xff2b2f33));
    g.fillRect (getLocalBounds().removeFromTop (toolbarHeight));

    if (range.isEmpty())
        return;

    // While a take is being auditioned the engine shows that take everywhere, so keep drawing the comp
    if (auditionRestoreTake < 0)
        sectionCache = getSections();

    const auto sections = sectionCache;
    g.setFont (juce::Font (juce::FontOptions (12.0f)));

    // Comp row
    {
        auto row = getRowArea (0);
        g.setColour (juce::Colours::white);
        auto labelArea = row.removeFromLeft (labelWidth);
        g.drawText ("Comp", labelArea.withTrimmedRight (playZone), juce::Justification::centredLeft);
        drawPlayButton (g, labelArea, auditioning == 0);

        for (auto& s : sections)
        {
            const int x1 = timeToX (s.start);
            const int x2 = timeToX (s.end);
            juce::Rectangle<int> block (x1, row.getY(), juce::jmax (1, x2 - x1), row.getHeight());

            g.setColour (colourForTake (s.take));
            g.fillRect (block);
            g.setColour (juce::Colours::black);
            g.drawRect (block);

            g.setColour (juce::Colours::white);
            g.drawText (s.take >= 0 ? "Take " + juce::String (s.take + 1) : juce::String ("Empty"),
                        block.reduced (4, 0), juce::Justification::centredLeft);
        }
    }

    // Take rows
    for (int i = 0; i < numTakes; ++i)
    {
        auto row = getRowArea (i + 1);

        g.setColour (colourForTake (i));
        auto labelArea = row.removeFromLeft (labelWidth);
        g.drawText ("Take " + juce::String (i + 1), labelArea.withTrimmedRight (playZone), juce::Justification::centredLeft);
        drawPlayButton (g, labelArea, auditioning == i + 1);

        g.setColour (juce::Colour (0xff2a2d31));
        g.fillRect (row);

        // Where this take is in use
        for (auto& s : sections)
        {
            if (s.take != i)
                continue;

            const int x1 = timeToX (s.start);
            const int x2 = timeToX (s.end);
            g.setColour (colourForTake (i).withAlpha (0.35f));
            g.fillRect (x1, row.getY(), juce::jmax (1, x2 - x1), row.getHeight());
        }

        if (i < thumbs.size() && thumbs[i] != nullptr)
        {
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            thumbs[i]->drawChannel (g, row.reduced (0, 4),
                                    te::TimeRange (te::TimePosition::fromSeconds (range.getStart()),
                                                   te::TimePosition::fromSeconds (range.getEnd())),
                                    0, 1.0f);
        }

        // Drag preview
        if (dragTake == i)
        {
            const int x1 = timeToX (juce::jmin (dragStart, dragEnd));
            const int x2 = timeToX (juce::jmax (dragStart, dragEnd));
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillRect (x1, row.getY(), juce::jmax (1, x2 - x1), row.getHeight());
        }
    }
}

void CompView::resized()
{
    flattenButton.setBounds (6, 5, 90, 24);
    cleanButton.setBounds (102, 5, 100, 24);
    hintLabel.setBounds (210, 5, getWidth() - 216, 24);
}

//==============================================================================
void CompView::mouseDown (const juce::MouseEvent& e)
{
    dragTake = -1;

    const int row = rowAt (e.y);
    const bool onPlay = e.x >= 6 + labelWidth - playZone && e.x < 6 + labelWidth;

    // The triangle listens: row 0 is the comp as it is, rows 1+ are one take on its own
    if (row >= 0 && onPlay)
    {
        if (auditioning == row)
        {
            session.stop();
            stopAudition();
        }
        else
        {
            startAudition (row);
        }

        return;
    }

    // Any other click puts the clip back on the comp first, so edits land on the comp
    stopAudition();

    if (row < 1)
        return;

    const int take = row - 1;

    // Clicking a take's name uses the whole take
    if (e.x < 6 + labelWidth)
    {
        useTakeForRange (take, range.getStart(), range.getEnd());
        return;
    }

    dragTake = take;
    dragStart = dragEnd = xToTime (e.x);
}

void CompView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragTake < 0)
        return;

    dragEnd = juce::jlimit (range.getStart(), range.getEnd(), xToTime (e.x));
    repaint();
}

void CompView::mouseUp (const juce::MouseEvent& e)
{
    if (dragTake < 0)
        return;

    const int take = dragTake;
    dragTake = -1;

    // A drag picks a range, a click swaps the section under the mouse
    if (e.getDistanceFromDragStart() > 3)
        useTakeForRange (take, juce::jmin (dragStart, dragEnd), juce::jmax (dragStart, dragEnd));
    else
        useTakeAt (take, dragStart);
}

//==============================================================================
CompWindow::CompWindow (EditSession& session, te::WaveAudioClip& c)
    : DocumentWindow ("Comp Takes", juce::Colour (0xff1e2124), DocumentWindow::allButtons),
      clip (&c)
{
    if (auto* t = c.getTrack())
        setName ("Comp Takes - " + t->getName());

    setUsingNativeTitleBar (true);
    setContentOwned (new CompView (session, c), true);
    setResizable (true, false);
    centreWithSize (juce::jmax (700, getWidth()), juce::jmax (220, getHeight()));
    setVisible (true);
}
