#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
void EditSession::play()
{
    getTransport().play (false);
}

void EditSession::record()
{
    if (! isAnyTrackArmed())
        setArmed (0, true);

    // Punching in: start 2 bars early so you hear the lead-in
    if (isPunchEnabled() && getLoopEnd() > getLoopStart() && ! getTransport().isPlaying())
    {
        const double bar = 4.0 * 60.0 / juce::jmax (1.0, getBpm());
        getTransport().setPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, getLoopStart() - bar * 2.0)));
    }

    getTransport().record (false);
}

void EditSession::stop()
{
    auto& transport = getTransport();
    const bool wasRecording = transport.isRecording();

    transport.stop (false, false);
    transport.setPosition (transport.looping.get() ? transport.getLoopRange().getStart() : te::TimePosition());

    if (wasRecording)
        save();
}

//==============================================================================
void EditSession::setBpm (double bpm)
{
    if (auto* tempo = edit->tempoSequence.getTempo (0))
        tempo->setBpm (juce::jlimit (40.0, 300.0, bpm));
}

double EditSession::getBpm() const
{
    return edit->tempoSequence.getBpmAt (te::TimePosition());
}

void EditSession::setClickEnabled (bool shouldBeOn)
{
    edit->clickTrackEnabled = shouldBeOn;
}

bool EditSession::isClickEnabled() const
{
    return edit->clickTrackEnabled.get();
}

void EditSession::setCountInBars (int bars)
{
    edit->setCountInMode (bars >= 2 ? te::Edit::CountIn::twoBar
                        : bars == 1 ? te::Edit::CountIn::oneBar
                                    : te::Edit::CountIn::none);
}

int EditSession::getCountInBars() const
{
    switch (edit->getCountInMode())
    {
        case te::Edit::CountIn::twoBar: return 2;
        case te::Edit::CountIn::oneBar: return 1;
        default:                        return 0;
    }
}

//==============================================================================
void EditSession::setLooping (bool shouldLoop)
{
    // No loop region yet, so default to 4 bars from the start
    if (shouldLoop && getLoopEnd() <= getLoopStart())
    {
        const double fourBars = 16.0 * 60.0 / juce::jmax (1.0, getBpm());
        setLoopRange (0.0, fourBars);
    }

    getTransport().looping = shouldLoop;
}

bool EditSession::isLooping()
{
    return getTransport().looping.get();
}

void EditSession::setLoopRange (double startSeconds, double endSeconds)
{
    if (endSeconds < startSeconds)
        std::swap (startSeconds, endSeconds);

    getTransport().setLoopRange (te::TimeRange (te::TimePosition::fromSeconds (juce::jmax (0.0, startSeconds)),
                                                te::TimePosition::fromSeconds (juce::jmax (0.0, endSeconds))));
}

double EditSession::getLoopStart()
{
    return getTransport().getLoopRange().getStart().inSeconds();
}

double EditSession::getLoopEnd()
{
    return getTransport().getLoopRange().getEnd().inSeconds();
}

//==============================================================================
void EditSession::undo()
{
    if (getTransport().isRecording())
        return;

    clearSelection();
    edit->undo();
    syncTrackState();
}

void EditSession::redo()
{
    if (getTransport().isRecording())
        return;

    clearSelection();
    edit->redo();
    syncTrackState();
}
