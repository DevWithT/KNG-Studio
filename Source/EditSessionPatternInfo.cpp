#include "EditSession.h"

namespace te = tracktion;

static constexpr int bankBar = 2000;      // the bank starts at this bar
static constexpr int bankSlotBars = 32;    // room for each pattern (patterns are up to 32 bars)

//==============================================================================
// Projects saved before the slots grew kept patterns 8 bars apart. Move them to the 32 bar slots, once.
static void moveOldBankSlots (EditSession& s)
{
    constexpr int oldSlotBars = 8;
    auto& state = s.getEdit().state;

    if ((int) state.getProperty ("kngBankSlotBars", oldSlotBars) == bankSlotBars)
        return;

    // Set first, so nothing below can run this twice
    state.setProperty ("kngBankSlotBars", bankSlotBars, nullptr);

    const double bankStart = s.barsAfter (0.0, bankBar) - 1.0;
    juce::Array<te::Clip*> bankClips;

    for (int i = 0; i < s.getNumTracks(); ++i)
        if (auto* t = s.getTrack (i))
            for (auto* c : t->getClips())
                if ((int) c->state.getProperty ("kngPattern", 0) > 0 && c->getPosition().getStart().inSeconds() >= bankStart)
                    bankClips.add (c);

    for (auto* c : bankClips)
    {
        const int pattern = (int) c->state.getProperty ("kngPattern", 0);
        double start = c->getPosition().getStart().inSeconds();

        if (pattern >= 2)
        {
            const double oldHome = s.barsAfter (0.0, bankBar + (pattern - 1) * oldSlotBars);
            const double newHome = s.barsAfter (0.0, bankBar + (pattern - 1) * bankSlotBars);
            start = newHome + (start - oldHome);
            c->setStart (te::TimePosition::fromSeconds (start), false, true);
        }

        // New patterns are 2 bars by default: an older 1 bar pattern grows to match
        const double wanted = s.barsAfter (start, s.getPatternBars (pattern));

        if (c->getPosition().getEnd().inSeconds() < wanted - 0.001)
            c->setEnd (te::TimePosition::fromSeconds (wanted), true);
    }

    s.getEdit().getUndoManager().clearUndoHistory();
}

double EditSession::getBankStart()
{
    return barsAfter (0.0, bankBar);
}

double EditSession::getPatternHome (int pattern)
{
    moveOldBankSlots (*this);

    // Every pattern has its own slot in the bank, so patterns never land on each other
    return barsAfter (0.0, bankBar + (juce::jmax (1, pattern) - 1) * bankSlotBars);
}

double EditSession::getSongEnd()
{
    const double bank = getBankStart() - 0.001;
    double end = 0.0;

    for (int i = 0; i < getNumTracks(); ++i)
        if (auto* t = getTrack (i))
            for (auto* c : t->getClips())
                if (c->getPosition().getStart().inSeconds() < bank)
                    end = juce::jmax (end, c->getPosition().getEnd().inSeconds());

    return end;
}

//==============================================================================
juce::String EditSession::getPatternName (int pattern)
{
    const auto name = edit->state.getProperty ("kngPatternName" + juce::String (pattern)).toString();
    return name.isNotEmpty() ? name : "Pattern " + juce::String (pattern);
}

void EditSession::setPatternName (int pattern, const juce::String& name)
{
    if (name.trim().isEmpty())
        return;

    beginEdit();
    edit->state.setProperty ("kngPatternName" + juce::String (pattern), name.trim(), &edit->getUndoManager());
    save();
}

juce::Colour EditSession::getPatternColour (int pattern)
{
    const juce::Identifier key ("kngPatternColour" + juce::String (pattern));

    if (edit->state.hasProperty (key))
        return juce::Colour::fromString (edit->state.getProperty (key).toString());

    return juce::Colour::fromHSV (std::fmod (0.58f + (pattern - 1) * 0.13f, 1.0f), 0.55f, 0.85f, 1.0f);
}

void EditSession::setPatternColour (int pattern, juce::Colour colour)
{
    beginEdit();
    edit->state.setProperty ("kngPatternColour" + juce::String (pattern), colour.toString(), &edit->getUndoManager());
    save();
}

int EditSession::duplicatePattern (int pattern)
{
    if (getTransport().isRecording())
        return -1;

    const int fresh = newPattern();
    beginEdit();

    edit->state.setProperty ("kngPatternBars" + juce::String (fresh), getPatternBars (pattern), nullptr);
    edit->state.setProperty ("kngPatternSwing" + juce::String (fresh), getPatternSwing (pattern), nullptr);
    edit->state.setProperty ("kngPatternName" + juce::String (fresh), getPatternName (pattern) + " copy", nullptr);
    edit->state.setProperty ("kngPatternColour" + juce::String (fresh), getPatternColour (pattern).toString(), nullptr);

    // Same notes on every sound, in the new pattern's own bank slot
    for (int i = 0; i < getNumTracks(); ++i)
        if (auto* src = getPatternClip (i, pattern, false))
            if (src->getSequence().getNumNotes() > 0)
                if (auto* dst = getPatternClip (i, fresh, true))
                    dst->getSequence().copyFrom (src->getSequence(), &edit->getUndoManager());

    save();
    return fresh;
}
