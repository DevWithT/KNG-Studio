#include "RackRow.h"
#include "PatternGrid.h"
#include "VolumeHelpers.h"
#include "KngSamplerPlugin.h"
#include "KngSynthPlugin.h"
#include "KngTheme.h"
#include <map>

namespace te = tracktion;

//==============================================================================
// For each bar of the current pattern: does any sound have a step or note in it?
static std::vector<bool> getLitBars (EditSession& session, int pattern, int perBar)
{
    std::vector<bool> lit (32, false);

    for (int i = 0; i < session.getNumTracks(); ++i)
    {
        if (auto* clip = session.getPatternClip (i, pattern, false))
        {
            const double start = PatternGrid::clipStartBeat (*clip);

            for (auto* n : clip->getSequence().getNotes())
            {
                const int bar = PatternGrid::beatToStep (n->getStartBeat().inBeats() - start) / perBar;

                if (juce::isPositiveAndBelow (bar, 32))
                    lit[(size_t) bar] = true;
            }
        }
    }

    return lit;
}

void RackRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (KngTheme::panel);
    g.fillRoundedRectangle (r, KngTheme::radius);
    g.setColour (KngTheme::line2.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), KngTheme::radius, 1.0f);

    // Track colour: a small pill on the left of the first line
    if (auto* at = getAudioTrack())
    {
        const auto c = at->getColour();
        g.setColour (c.getAlpha() > 0 ? c.withAlpha (1.0f) : KngTheme::laneGrey);
        g.fillRoundedRectangle (r.getX() + 4.0f, r.getY() + 8.0f, 3.0f, (float) rowHeight - 16.0f, 1.5f);
    }

    if (getTrackIndex() < 0)
        return;

    auto* clip = getClip (false);

    if (isChannel())
    {
        paintChannelSteps (g, clip);
    }
    else if (isDrum())
    {
        paintDrumSummary (g, clip);

        if (isExpanded())
            paintDrumRows (g, clip);
    }
    else
    {
        paintMelodic (g, clip);
    }

    paintGraph (g, clip);

    // A bar with no step or note is dimmed (bar 1 never is). Dimmed bars past the pattern's end are locked: a step placed in one unlocks it.
    {
        const int pattern = session.getCurrentPattern();
        const int live = session.getPatternSteps (pattern);
        const int shown = getNumSteps();
        const int perBar = juce::jmax (1, live / juce::jmax (1, session.getPatternBars (pattern)));
        const auto lit = getLitBars (session, pattern, perBar);
        auto lane = getLaneArea();

        g.setFont (juce::Font (juce::FontOptions (10.0f)));

        for (int s = 0, bar = 0; s < shown; s += perBar, ++bar)
        {
            if (bar == 0 || (bar < (int) lit.size() && lit[(size_t) bar]))
                continue;

            const int x1 = lane.getX() + s * lane.getWidth() / shown;
            const int x2 = lane.getX() + juce::jmin (shown, s + perBar) * lane.getWidth() / shown;

            g.setColour (KngTheme::panel.withAlpha (0.65f));
            g.fillRect (x1, 0, x2 - x1, getHeight());

            g.setColour (KngTheme::muted);
            g.drawText (juce::String (bar + 1), x1 + 4, 1, 28, 12, juce::Justification::centredLeft, false);
        }

        // A thin line where the pattern ends
        if (live < shown)
        {
            g.setColour (KngTheme::line2);
            g.fillRect (lane.getX() + live * lane.getWidth() / shown, 4, 1, getHeight() - 8);
        }
    }
}

void RackRow::paintDrumSummary (juce::Graphics& g, te::MidiClip* clip)
{
    const int steps = getNumSteps();
    auto lane = getLaneArea().withHeight (rowHeight).reduced (0, 9);
    const int playStep = getPlayStep (clip);

    // Which steps have any hit at all
    bool hit[maxSteps] = {};

    if (clip != nullptr)
    {
        const double start = PatternGrid::clipStartBeat (*clip);

        for (auto* n : clip->getSequence().getNotes())
        {
            const int step = PatternGrid::beatToStep (n->getStartBeat().inBeats() - start);

            if (juce::isPositiveAndBelow (step, steps))
                hit[step] = true;
        }
    }

    for (int step = 0; step < steps; ++step)
    {
        const int x1 = lane.getX() + step * lane.getWidth() / steps;
        const int x2 = lane.getX() + (step + 1) * lane.getWidth() / steps;
        auto cell = juce::Rectangle<int> (x1, lane.getY(), x2 - x1, lane.getHeight()).reduced (1).toFloat();

        g.setColour (hit[step]            ? KngTheme::accent
                   : (step / 4) % 2 == 0  ? KngTheme::chrome
                                          : KngTheme::btn);
        g.fillRoundedRectangle (cell, 2.0f);

        if (step == playStep)
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillRoundedRectangle (cell, 2.0f);
        }
    }
}

void RackRow::paintDrumRows (juce::Graphics& g, te::MidiClip* clip)
{
    const int idx = getTrackIndex();
    const int steps = getNumSteps();
    const int playStep = getPlayStep (clip);
    auto pads = getVisiblePads();

    if (pads.isEmpty())
    {
        g.setColour (KngTheme::muted);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("Drop a kit folder here from the Browser, or click Load Kit.",
                    getLocalBounds().withTrimmedTop (rowHeight).withHeight (padRowHeight).reduced (30, 0),
                    juce::Justification::centredLeft);
        return;
    }

    // Velocity of every hit, by pad and step
    int velocity[16][maxSteps] = {};

    if (clip != nullptr)
    {
        const double start = PatternGrid::clipStartBeat (*clip);

        for (auto* n : clip->getSequence().getNotes())
        {
            const int pad  = n->getNoteNumber() - EditSession::getPadNote (0);
            const int step = PatternGrid::beatToStep (n->getStartBeat().inBeats() - start);

            if (juce::isPositiveAndBelow (pad, 16) && juce::isPositiveAndBelow (step, steps))
                velocity[pad][step] = n->getVelocity();
        }
    }

    g.setFont (juce::Font (juce::FontOptions (12.0f)));

    for (int sub = 0; sub < pads.size(); ++sub)
    {
        const int pad = pads[sub];
        const int y = rowHeight + sub * padRowHeight;

        g.setColour (KngTheme::text);
        g.drawFittedText (session.getPadName (idx, pad), 30, y, headerWidth - 40, padRowHeight,
                          juce::Justification::centredLeft, 1);

        for (int step = 0; step < steps; ++step)
        {
            auto cell = getStepBounds (sub, step).reduced (2).toFloat();

            if (velocity[pad][step] > 0)
            {
                const float v = velocity[pad][step] / 127.0f;
                g.setColour (KngTheme::accent.darker (0.6f * (1.0f - v)));
            }
            else
            {
                g.setColour ((step / 4) % 2 == 0 ? KngTheme::chrome : KngTheme::btn);
            }

            g.fillRoundedRectangle (cell, 3.0f);

            if (step == playStep)
            {
                g.setColour (juce::Colours::white.withAlpha (0.16f));
                g.fillRoundedRectangle (cell, 3.0f);
            }
        }
    }
}

void RackRow::paintMelodic (juce::Graphics& g, te::MidiClip* clip)
{
    auto lane = getLaneArea().reduced (0, 5);

    g.setColour (KngTheme::inset);
    g.fillRoundedRectangle (lane.toFloat(), 4.0f);

    if (clip == nullptr || clip->getSequence().getNumNotes() == 0)
    {
        g.setColour (KngTheme::muted);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("Click to write notes for this pattern", lane, juce::Justification::centred);
        return;
    }

    // Mini piano roll of the pattern
    const double start = PatternGrid::clipStartBeat (*clip);
    const double lengthBeats = getNumSteps() * PatternGrid::stepBeats;
    const auto range = clip->getSequence().getNoteNumberRange();
    const int lo = range.getStart() - 1;
    const float span = (float) juce::jmax (8, range.getEnd() + 1 - lo);

    g.setColour (KngTheme::accent);

    for (auto* n : clip->getSequence().getNotes())
    {
        const double b1 = n->getStartBeat().inBeats() - start;
        const double b2 = b1 + n->getLengthBeats().inBeats();

        if (b2 <= 0.0 || b1 >= lengthBeats)
            continue;

        const float x1 = lane.getX() + (float) (juce::jmax (0.0, b1) / lengthBeats) * lane.getWidth();
        const float x2 = lane.getX() + (float) (juce::jmin (lengthBeats, b2) / lengthBeats) * lane.getWidth();
        const float y  = lane.getY() + 2.0f + (1.0f - (n->getNoteNumber() - lo) / span) * (lane.getHeight() - 6.0f);

        g.fillRoundedRectangle (x1, y, juce::jmax (2.0f, x2 - x1 - 1.0f), 3.0f, 1.0f);
    }

    // Playhead
    const int playStep = getPlayStep (clip);

    if (playStep >= 0)
    {
        const double beat = clip->getContentBeatAtTime (session.getTransport().getPosition()).inBeats() - start;
        const int x = lane.getX() + juce::roundToInt (beat / lengthBeats * lane.getWidth());
        g.setColour (KngTheme::accent);
        g.fillRect (x, lane.getY(), 2, lane.getHeight());
    }
}

void RackRow::resized()
{
    auto r = juce::Rectangle<int> (0, 0, headerWidth, rowHeight).reduced (8, 6);
    r.removeFromLeft (2);

    auto small = [] (juce::Rectangle<int> b) { return b.withSizeKeepingCentre (b.getWidth(), 24); };

    expandButton.setBounds (small (r.removeFromLeft (22)));
    r.removeFromLeft (4);
    instrumentButton.setBounds (small (r.removeFromRight (70)));
    r.removeFromRight (4);
    panKnob.setBounds (r.removeFromRight (28));
    volKnob.setBounds (r.removeFromRight (28));
    r.removeFromRight (4);
    soloButton.setBounds (small (r.removeFromRight (22)));
    r.removeFromRight (2);
    muteButton.setBounds (small (r.removeFromRight (22)));
    r.removeFromRight (2);
    armButton.setBounds (small (r.removeFromRight (22)));
    r.removeFromRight (4);
    nameLabel.setBounds (r);
}
