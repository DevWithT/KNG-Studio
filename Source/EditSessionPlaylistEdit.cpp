#include "EditSession.h"
#include <map>
#include <algorithm>

namespace te = tracktion;

//==============================================================================
void EditSession::cutBlock (const PatternBlock& block, double atSeconds)
{
    if (getTransport().isRecording())
        return;

    const double t = snapEnabled ? snapTime (atSeconds) : atSeconds;

    if (t <= block.start + 0.01 || t >= block.end - 0.01)
        return;

    beginEdit();

    // Both halves keep the pattern, its lane and its link, so they stay in sync with the Rack
    for (auto* c : getBlockClips (block))
    {
        const auto pos = c->getPosition();

        if (t > pos.getStart().inSeconds() + 0.001 && t < pos.getEnd().inSeconds() - 0.001)
            if (auto* ct = c->getClipTrack())
                ct->splitClip (*c, te::TimePosition::fromSeconds (t));
    }

    save();
}

void EditSession::resizeBlock (const PatternBlock& block, double newEnd)
{
    if (getTransport().isRecording())
        return;

    auto& ts = edit->tempoSequence;

    // At least one beat long
    const auto startBeat = ts.toBeats (te::TimePosition::fromSeconds (block.start));
    newEnd = juce::jmax (ts.toTime (startBeat + te::BeatDuration::fromBeats (1.0)).inSeconds(), newEnd);

    // One pass of the pattern, in beats
    const double home = getPatternHome (block.pattern);
    const double patternBeats = ts.toBeats (te::TimePosition::fromSeconds (barsAfter (home, getPatternBars (block.pattern)))).inBeats()
                              - ts.toBeats (te::TimePosition::fromSeconds (home)).inBeats();

    beginEdit();

    for (auto* c : getBlockClips (block))
    {
        // Longer than the pattern: repeat it
        if (auto* mc = dynamic_cast<te::MidiClip*> (c))
            if (newEnd > c->getPosition().getEnd().inSeconds() + 0.001 && ! mc->isLooping() && patternBeats > 0.0)
                mc->setLoopRangeBeats (te::BeatRange (te::BeatPosition(), te::BeatDuration::fromBeats (patternBeats)));

        c->setEnd (te::TimePosition::fromSeconds (newEnd), true);
    }

    save();
}

//==============================================================================
void EditSession::tidyPlaylist()
{
    if (getTransport().isRecording())
        return;

    const double bank = getBankStart() - 0.001;

    // 1. Loose MIDI clips (recorded, chopped, older projects) become pattern blocks.
    //    Clips that start and end together become one pattern.
    struct Loose { te::MidiClip* clip; int track; };
    std::vector<Loose> loose;

    for (int i = 0; i < getNumTracks(); ++i)
        if (isMidiTrack (i))
            if (auto* t = getTrack (i))
                for (auto* c : t->getClips())
                    if (auto* mc = dynamic_cast<te::MidiClip*> (c))
                        if (! mc->state.hasProperty ("kngPattern") && mc->getPosition().getStart().inSeconds() < bank)
                            loose.push_back ({ mc, i });

    if (! loose.empty())
    {
        const int keep = currentPattern;
        const auto lane = getDefaultLaneId();
        std::vector<bool> done (loose.size(), false);

        for (size_t a = 0; a < loose.size(); ++a)
        {
            if (done[a])
                continue;

            auto* first = loose[a].clip;
            const double s = first->getPosition().getStart().inSeconds();
            const double e = first->getPosition().getEnd().inSeconds();

            const int p = newPattern();
            auto* firstTrack = getTrack (loose[a].track);
            // A placeholder clip name ("New MIDI Clip") gives way to the sound's name
            const auto clipName = first->getName();
            const bool generic = clipName.isEmpty() || clipName.startsWithIgnoreCase ("New MIDI")
                                  || clipName.startsWithIgnoreCase ("Pattern");
            const auto name = ! generic ? clipName
                            : firstTrack != nullptr ? firstTrack->getName() : juce::String ("Recording");

            const int bars = juce::jlimit (1, 4, barNumberAt (juce::jmax (s, e - 0.001)) - barNumberAt (s) + 1);
            edit->state.setProperty ("kngPatternName" + juce::String (p), name, nullptr);
            edit->state.setProperty ("kngPatternBars" + juce::String (p), bars, nullptr);

            for (size_t b = a; b < loose.size(); ++b)
            {
                auto* c = loose[b].clip;

                if (done[b] || std::abs (c->getPosition().getStart().inSeconds() - s) > 0.001
                            || std::abs (c->getPosition().getEnd().inSeconds() - e) > 0.001)
                    continue;

                done[b] = true;
                c->state.setProperty ("kngPattern", p, nullptr);
                c->state.setProperty ("kngLane", lane, nullptr);

                if (auto* t = getTrack (loose[b].track))
                    c->setLinkGroupID ("kngpat" + juce::String (p) + "_" + t->itemID.toString());
            }
        }

        currentPattern = keep;
        save();
    }

    // Patterns adopted earlier with a placeholder name take their sound's name
    for (int p = 1; p <= getNumPatterns(); ++p)
    {
        if (! getPatternName (p).startsWithIgnoreCase ("New MIDI"))
            continue;

        for (int i = 0; i < getNumTracks(); ++i)
        {
            if (getPatternClip (i, p, false) != nullptr)
            {
                if (auto* t = getTrack (i))
                    edit->state.setProperty ("kngPatternName" + juce::String (p), t->getName(), nullptr);

                break;
            }
        }
    }

    // 2. A muted lane silences its blocks
    for (auto& b : getPatternBlocks())
    {
        const int laneTrack = getLaneTrackIndex (b.lane);
        auto* t = laneTrack >= 0 ? getTrack (laneTrack) : nullptr;
        const bool muted = t != nullptr && t->isMuted (false);

        for (auto* c : getBlockClips (b))
            if (c->disabled.get() != muted)
                c->disabled = muted;
    }

    // Bank copies always play when you loop a pattern
    for (int i = 0; i < getNumTracks(); ++i)
        if (auto* t = getTrack (i))
            for (auto* c : t->getClips())
                if (c->getPosition().getStart().inSeconds() >= bank && c->disabled.get())
                    c->disabled = false;
}

//==============================================================================
void EditSession::separateOverlappingBlocks()
{
    if (getTransport().isRecording())
        return;

    auto blocks = getPatternBlocks();

    std::sort (blocks.begin(), blocks.end(), [] (const PatternBlock& a, const PatternBlock& b)
    {
        return a.start < b.start;
    });

    std::map<juce::String, std::vector<PatternBlock>> kept;   // blocks that stay, per lane
    std::map<juce::String, juce::String> overflow;            // each lane's "More Patterns" lane

    for (auto& b : blocks)
    {
        auto& laneKept = kept[b.lane];
        bool clash = false;

        for (auto& k : laneKept)
        {
            if (k.start < b.end - 0.001 && k.end > b.start + 0.001)
            {
                clash = true;
                break;
            }
        }

        if (! clash)
        {
            laneKept.push_back (b);
            continue;
        }

        auto& target = overflow[b.lane];

        if (target.isEmpty())
        {
            addTrack();
            const int idx = getNumTracks() - 1;

            if (auto* t = getTrack (idx))
                t->setName ("More Patterns");

            target = getLaneId (idx);
        }

        moveBlock (b, b.start, target, false);
        kept[target].push_back ({ b.pattern, b.start, b.end, target });
    }
}
