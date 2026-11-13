#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
void EditSession::setView (double startSeconds, double lengthSeconds)
{
    viewLength = juce::jlimit (1.0, 600.0, lengthSeconds);
    viewStart  = juce::jmax (0.0, startSeconds);
}

void EditSession::zoomAround (double timeSeconds, double factor)
{
    const double newLength = juce::jlimit (1.0, 600.0, viewLength * factor);
    const double ratio = newLength / viewLength;

    // Keep the time under the mouse in the same spot on screen
    setView (timeSeconds - (timeSeconds - viewStart) * ratio, newLength);
}

void EditSession::scrollBy (double seconds)
{
    setView (viewStart + seconds, viewLength);
}

double EditSession::xToTime (int x, int width) const
{
    if (width <= 0)
        return viewStart;

    return viewStart + x / (double) width * viewLength;
}

int EditSession::timeToX (double seconds, int width) const
{
    return juce::roundToInt ((seconds - viewStart) / viewLength * width);
}

double EditSession::snapTime (double seconds) const
{
    if (! snapEnabled)
        return seconds;

    auto& ts = edit->tempoSequence;
    const auto t = te::TimePosition::fromSeconds (juce::jmax (0.0, seconds));
    const int grid = getSnapGrid();

    // Whole bars: the nearest bar line
    if (grid <= 0)
    {
        const auto bb = ts.toBarsAndBeats (t);
        const double a = ts.toTime (te::tempo::BarsAndBeats { bb.bars, {} }).inSeconds();
        const double b = ts.toTime (te::tempo::BarsAndBeats { bb.bars + 1, {} }).inSeconds();
        return seconds - a < b - seconds ? a : b;
    }

    // Nearest grid line on the tempo map, so it stays right across tempo changes
    const double beat = std::round (ts.toBeats (t).inBeats() * grid) / grid;
    return ts.toTime (te::BeatPosition::fromBeats (beat)).inSeconds();
}

int EditSession::getSnapGrid() const
{
    const int grid = (int) edit->state.getProperty ("kngSnapGrid", 1);
    return (grid == 0 || grid == 1 || grid == 2 || grid == 4 || grid == 8) ? grid : 1;
}

void EditSession::setSnapGrid (int perBeat)
{
    edit->state.setProperty ("kngSnapGrid", perBeat, nullptr);
    save();
}

//==============================================================================
void EditSession::beginEdit()
{
    edit->getUndoManager().beginNewTransaction();
}

void EditSession::splitSelectedClipAtPlayhead()
{
    if (getTransport().isRecording())
        return;

    const auto t = getTransport().getPosition();
    std::vector<te::Clip::Ptr> rightHalves;
    bool started = false;

    // Split every selected clip the playhead is inside
    for (auto& c : getSelectedClips())
    {
        const auto pos = c->getPosition();

        if (t <= pos.getStart() || t >= pos.getEnd())
            continue;

        if (auto* clipTrack = c->getClipTrack())
        {
            if (! started)
            {
                beginEdit();
                started = true;
            }

            if (auto* right = clipTrack->splitClip (*c, t))
                rightHalves.push_back (right);
        }
    }

    if (! started)
        return;

    // Select the right-hand halves so you can keep splitting forward
    clearSelection();

    for (auto& r : rightHalves)
        selection.push_back (r);

    if (! rightHalves.empty())
        selectedClip = rightHalves.back();

    save();
}
