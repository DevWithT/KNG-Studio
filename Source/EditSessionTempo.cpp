#include "EditSession.h"

namespace te = tracktion;

static te::TimePosition tp (double seconds)
{
    return te::TimePosition::fromSeconds (juce::jmax (0.0, seconds));
}

double EditSession::barStartAt (double seconds)
{
    auto& ts = edit->tempoSequence;
    const auto bb = ts.toBarsAndBeats (tp (seconds));
    return ts.toTime (te::tempo::BarsAndBeats { bb.bars, {} }).inSeconds();
}

double EditSession::barsAfter (double barStart, int bars)
{
    auto& ts = edit->tempoSequence;
    const auto bb = ts.toBarsAndBeats (tp (barStart));
    return ts.toTime (te::tempo::BarsAndBeats { bb.bars + bars, {} }).inSeconds();
}

int EditSession::barNumberAt (double seconds)
{
    return edit->tempoSequence.toBarsAndBeats (tp (seconds)).bars + 1;
}

double EditSession::snapToBeat (double seconds)
{
    auto& ts = edit->tempoSequence;
    const double beat = std::round (ts.toBeats (tp (seconds)).inBeats());
    return ts.toTime (te::BeatPosition::fromBeats (beat)).inSeconds();
}

double EditSession::getBpmAt (double seconds)
{
    return edit->tempoSequence.getBpmAt (tp (seconds));
}

void EditSession::setBpmAt (double seconds, double bpm)
{
    edit->tempoSequence.getTempoAt (tp (seconds)).setBpm (juce::jlimit (40.0, 300.0, bpm));
}

juce::String EditSession::getTimeSigAt (double seconds)
{
    return edit->tempoSequence.getTimeSigAt (tp (seconds)).getStringTimeSig();
}

void EditSession::addTempoChange (double seconds, double bpm)
{
    auto& ts = edit->tempoSequence;
    const double start = barStartAt (seconds);

    beginEdit();

    // At the very start, change the song's first tempo instead of stacking a second one there
    if (start < 0.0005)
        setBpm (bpm);
    else
        ts.insertTempo (ts.toBeats (tp (start)), juce::jlimit (40.0, 300.0, bpm), 0.0f);

    save();
}

void EditSession::addTimeSigChange (double seconds, int numerator, int denominator)
{
    auto& ts = edit->tempoSequence;
    const double start = barStartAt (seconds);
    const auto text = juce::String (numerator) + "/" + juce::String (denominator);

    beginEdit();

    if (start < 0.0005)
    {
        if (auto* first = ts.getTimeSig (0))
            first->setStringTimeSig (text);
    }
    else if (auto sig = ts.insertTimeSig (ts.toBeats (tp (start))))
    {
        sig->setStringTimeSig (text);
    }

    save();
}

std::vector<EditSession::TempoMark> EditSession::getTempoMarks()
{
    auto& ts = edit->tempoSequence;
    std::vector<TempoMark> marks;

    // The first tempo and time signature are the song's own, so only later changes are marks
    for (int i = 1; i < ts.getNumTempos(); ++i)
        if (auto* t = ts.getTempo (i))
            marks.push_back ({ ts.toTime (t->getStartBeat()).inSeconds(),
                               juce::String (juce::roundToInt (t->getBpm())) + " BPM", false, i });

    for (int i = 1; i < ts.getNumTimeSigs(); ++i)
        if (auto* s = ts.getTimeSig (i))
            marks.push_back ({ ts.toTime (s->getStartBeat()).inSeconds(), s->getStringTimeSig(), true, i });

    return marks;
}

void EditSession::removeTempoMark (const TempoMark& mark)
{
    auto& ts = edit->tempoSequence;
    beginEdit();

    if (mark.isTimeSig)
        ts.removeTimeSig (mark.index);
    else
        ts.removeTempo (mark.index, false);

    save();
}
