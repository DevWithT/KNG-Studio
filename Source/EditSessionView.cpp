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

    const double beat = 60.0 / juce::jmax (1.0, getBpm());
    return std::round (seconds / beat) * beat;
}

//==============================================================================
void EditSession::beginEdit()
{
    edit->getUndoManager().beginNewTransaction();
}

void EditSession::splitSelectedClipAtPlayhead()
{
    if (selectedClip == nullptr || getTransport().isRecording())
        return;

    const auto t = getTransport().getPosition();
    const auto pos = selectedClip->getPosition();

    if (t <= pos.getStart() || t >= pos.getEnd())
        return;

    if (auto* clipTrack = selectedClip->getClipTrack())
    {
        beginEdit();

        // Select the right-hand half so you can keep splitting forward
        selectedClip = clipTrack->splitClip (*selectedClip, t);
        save();
    }
}
