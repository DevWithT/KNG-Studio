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

    // Rec always records into the song. Pattern mode keeps the playhead in the pattern bank (bar 2000+),
    // where a take would land out of sight, so go back to Song mode first.
    if (isPatternMode())
        setPatternMode (false);

    const double bank = getBankStart() - 1.0;

    // A loop left over from Pattern mode sits in the bank, and the engine would jump the take there: drop it
    if (isLooping() && getLoopStart() >= bank)
        setLooping (false);

    if (getTransport().getPosition().inSeconds() >= bank)
        getTransport().setPosition (te::TimePosition());

    // The bank only holds pattern notes: clear out any audio takes that landed there by mistake
    {
        juce::Array<te::Clip*> strays;

        for (int i = 0; i < getNumTracks(); ++i)
            if (auto* t = getTrack (i))
                for (auto* c : t->getClips())
                    if (dynamic_cast<te::WaveAudioClip*> (c) != nullptr && c->getPosition().getStart().inSeconds() >= bank)
                        strays.add (c);

        if (! strays.isEmpty())
        {
            beginEdit();

            for (auto* c : strays)
                c->removeFromParent();
        }
    }

    // Send every arm to the engine again, in case a device or track change dropped one
    for (int i = 0; i < (int) trackArmed.size(); ++i)
        if (trackArmed[(size_t) i])
            applyArm (i, true);

    // Punching in: start 2 bars early so you hear the lead-in
    if (isPunchEnabled() && getLoopEnd() > getLoopStart() && ! getTransport().isPlaying())
    {
        const double bar = 4.0 * 60.0 / juce::jmax (1.0, getBpm());
        getTransport().setPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, getLoopStart() - bar * 2.0)));
    }

    // Remember where this take starts and what was already there, for Re-take
    recordStartSeconds = getTransport().getPosition().inSeconds();
    clipsBeforeRecording.clear();

    for (int i = 0; i < getNumTracks(); ++i)
        if (auto* t = getTrack (i))
            for (auto* c : t->getClips())
                clipsBeforeRecording.push_back (c->itemID);

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
    const double bank = getBankStart() - 1.0;

    // No loop yet, or one left in the pattern bank while in Song mode: 4 bars from the bar you're on
    if (shouldLoop && (getLoopEnd() <= getLoopStart() || (! patternMode && getLoopStart() >= bank)))
    {
        double from = getTransport().getPosition().inSeconds();

        if (from >= bank)
        {
            from = 0.0;
            getTransport().setPosition (te::TimePosition());
        }

        const double start = barStartAt (from);
        setLoopRange (start, barsAfter (start, 4));
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

void EditSession::retake()
{
    auto& transport = getTransport();

    if (! transport.isRecording())
        return;

    transport.stop (false, false);

    // The take is written when recording stops; remove it on the next tick, then go again
    juce::MessageManager::callAsync ([this]
    {
        beginEdit();

        for (int i = 0; i < getNumTracks(); ++i)
        {
            if (auto* t = getTrack (i))
            {
                juce::Array<te::Clip*> fresh;

                for (auto* c : t->getClips())
                    if (std::find (clipsBeforeRecording.begin(), clipsBeforeRecording.end(), c->itemID) == clipsBeforeRecording.end())
                        fresh.add (c);

                for (auto* c : fresh)
                    c->removeFromParent();
            }
        }

        clearSelection();
        getTransport().setPosition (te::TimePosition::fromSeconds (recordStartSeconds));
        record();
    });
}
