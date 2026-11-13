#include "RackRow.h"
#include "PatternGrid.h"
#include "VolumeHelpers.h"
#include "KngSamplerPlugin.h"
#include "KngSynthPlugin.h"
#include <map>

namespace te = tracktion;

//==============================================================================
void RackRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff24282c));
    g.fillRoundedRectangle (r, 5.0f);

    // Track colour along the left edge
    if (auto* at = getAudioTrack())
    {
        const auto c = at->getColour();
        g.setColour (c.getAlpha() > 0 ? c.withAlpha (1.0f) : juce::Colour (0xff3a8ee6));
        g.fillRoundedRectangle (r.withWidth (5.0f), 2.0f);
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
}

void RackRow::paintDrumSummary (juce::Graphics& g, te::MidiClip* clip)
{
    const int steps = getNumSteps();
    auto lane = getLaneArea().withHeight (rowHeight).reduced (0, 9);
    const int playStep = getPlayStep (clip);

    // Which steps have any hit at all
    bool hit[128] = {};

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

        g.setColour (hit[step]            ? juce::Colour (0xff3a8ee6)
                   : (step / 4) % 2 == 0  ? juce::Colour (0xff30353a)
                                          : juce::Colour (0xff3a3034));
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
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("Drop a kit folder here from the Browser, or click Load Kit.",
                    getLocalBounds().withTrimmedTop (rowHeight).withHeight (padRowHeight).reduced (30, 0),
                    juce::Justification::centredLeft);
        return;
    }

    // Velocity of every hit, by pad and step
    int velocity[16][128] = {};

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

        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawFittedText (session.getPadName (idx, pad), 30, y, headerWidth - 40, padRowHeight,
                          juce::Justification::centredLeft, 1);

        for (int step = 0; step < steps; ++step)
        {
            auto cell = getStepBounds (sub, step).reduced (2).toFloat();

            if (velocity[pad][step] > 0)
            {
                const float v = velocity[pad][step] / 127.0f;
                g.setColour (juce::Colour (0xff3a8ee6).interpolatedWith (juce::Colour (0xff9fd4ff), v * 0.6f));
            }
            else
            {
                g.setColour ((step / 4) % 2 == 0 ? juce::Colour (0xff353a40) : juce::Colour (0xff43363a));
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

    g.setColour (juce::Colour (0xff1b1d20));
    g.fillRoundedRectangle (lane.toFloat(), 4.0f);

    if (clip == nullptr || clip->getSequence().getNumNotes() == 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
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

    g.setColour (juce::Colour (0xff3a8ee6));

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
        g.setColour (juce::Colours::yellow);
        g.fillRect (x, lane.getY(), 2, lane.getHeight());
    }
}

void RackRow::resized()
{
    auto r = juce::Rectangle<int> (0, 0, headerWidth, rowHeight).reduced (8, 6);
    r.removeFromLeft (2);

    expandButton.setBounds (r.removeFromLeft (22));
    r.removeFromLeft (4);
    instrumentButton.setBounds (r.removeFromRight (70));
    r.removeFromRight (4);
    panKnob.setBounds (r.removeFromRight (24));
    volKnob.setBounds (r.removeFromRight (24));
    r.removeFromRight (4);
    soloButton.setBounds (r.removeFromRight (22));
    r.removeFromRight (2);
    muteButton.setBounds (r.removeFromRight (22));
    r.removeFromRight (2);
    armButton.setBounds (r.removeFromRight (22));
    r.removeFromRight (4);
    nameLabel.setBounds (r);
}
