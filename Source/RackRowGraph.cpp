#include "RackRow.h"
#include "PatternGrid.h"
#include <algorithm>

namespace te = tracktion;

static int stepOf (te::MidiClip& clip, te::MidiNote& n)
{
    return PatternGrid::beatToStep (n.getStartBeat().inBeats() - PatternGrid::clipStartBeat (clip));
}

//==============================================================================
int RackRow::getGraphMode()
{
    if (! isChannel() && ! isDrum())
        return 0;

    const int mode = (int) track->state.getProperty ("kngGraph", 0);

    // Pitch only makes sense on sound rows (on a kit, a different note is a different pad)
    return isDrum() && mode == 2 ? 1 : juce::jlimit (0, 2, mode);
}

int RackRow::getGraphNote()
{
    if (! isDrum())
        return -1;   // sound rows: whatever note is on the step

    auto pads = getVisiblePads();
    const int pad = (int) track->state.getProperty ("kngGraphPad", pads.isEmpty() ? 0 : pads.getFirst());
    return EditSession::getPadNote (pad);
}

juce::Rectangle<int> RackRow::getGraphArea()
{
    const int top = isDrum() && isExpanded() ? rowHeight + juce::jmax (1, getVisiblePads().size()) * padRowHeight + 6
                                             : rowHeight;
    auto lane = getLaneArea();
    return { lane.getX(), top + 2, lane.getWidth(), graphHeight - 6 };
}

te::MidiNote* RackRow::noteAtStep (te::MidiClip& clip, int step, int pitch)
{
    for (auto* n : clip.getSequence().getNotes())
        if (stepOf (clip, *n) == step && (pitch < 0 || n->getNoteNumber() == pitch))
            return n;

    return nullptr;
}

void RackRow::setChannelStep (te::MidiClip& clip, int step, bool on)
{
    auto* um = &session.getEdit().getUndoManager();

    if (on)
    {
        if (noteAtStep (clip, step, -1) == nullptr)
            PatternGrid::setStep (clip, getChannelNote(), step, true, um);

        return;
    }

    // Off: whatever pitch the step was moved to
    juce::Array<te::MidiNote*> doomed;

    for (auto* n : clip.getSequence().getNotes())
        if (stepOf (clip, *n) == step)
            doomed.add (n);

    for (auto* n : doomed)
        clip.getSequence().removeNote (*n, um);
}

void RackRow::makeChannelLegato (te::MidiClip& clip)
{
    // Each hit rings until the next one, whatever its pitch
    auto* um = &session.getEdit().getUndoManager();
    const auto notes = clip.getSequence().getNotes();
    const double end = PatternGrid::clipStartBeat (clip) + getNumSteps() * PatternGrid::stepBeats;
    std::vector<double> starts;

    for (auto* n : notes)
        starts.push_back (n->getStartBeat().inBeats());

    std::sort (starts.begin(), starts.end());

    for (auto* n : notes)
    {
        const double s = n->getStartBeat().inBeats();
        auto next = std::upper_bound (starts.begin(), starts.end(), s + 1.0e-4);
        const double length = (next != starts.end() ? *next : end) - s;
        n->setStartAndLength (n->getStartBeat(), te::BeatDuration::fromBeats (juce::jmax (0.02, length)), um);
    }
}

//==============================================================================
void RackRow::paintGraph (juce::Graphics& g, te::MidiClip* clip)
{
    const int mode = getGraphMode();

    if (mode == 0)
        return;

    auto area = getGraphArea();
    const int steps = getNumSteps();
    const int pitch = getGraphNote();
    const int root = getChannelNote();

    // What the graph is drawing, on the left
    juce::String label = mode == 2 ? "Pitch" : "Velocity";

    if (isDrum())
    {
        const int pad = pitch - EditSession::getPadNote (0);
        label << "\n" << session.getPadName (getTrackIndex(), pad);
    }

    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawFittedText (label, 30, area.getY(), headerWidth - 40, area.getHeight(), juce::Justification::centredLeft, 2);

    g.setColour (juce::Colour (0xff1b1d20));
    g.fillRoundedRectangle (area.toFloat(), 4.0f);

    if (mode == 2)
    {
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawHorizontalLine (area.getCentreY(), (float) area.getX(), (float) area.getRight());
    }

    const juce::Colour barColour = mode == 2 ? juce::Colour (0xffb07cff) : juce::Colour (0xffe6893a);

    for (int step = 0; step < steps; ++step)
    {
        const int x1 = area.getX() + step * area.getWidth() / steps;
        const int x2 = area.getX() + (step + 1) * area.getWidth() / steps;
        auto bar = juce::Rectangle<int> (x1, area.getY(), x2 - x1, area.getHeight()).reduced (2, 4).toFloat();
        auto* n = clip != nullptr ? noteAtStep (*clip, step, pitch) : nullptr;

        if (n == nullptr)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRect (bar.withSizeKeepingCentre (bar.getWidth(), 2.0f).withY (mode == 2 ? bar.getCentreY() - 1.0f : bar.getBottom() - 2.0f));
            continue;
        }

        if (mode == 1)
        {
            const float h = juce::jmax (2.0f, n->getVelocity() / 127.0f * bar.getHeight());
            g.setColour (barColour);
            g.fillRoundedRectangle (bar.withTop (bar.getBottom() - h), 2.0f);
        }
        else
        {
            const int offset = juce::jlimit (-12, 12, n->getNoteNumber() - root);
            const float half = bar.getHeight() * 0.5f;
            const float h = juce::jmax (2.0f, std::abs (offset) / 12.0f * half);
            const float top = offset >= 0 ? bar.getCentreY() - h : bar.getCentreY();

            g.setColour (barColour);
            g.fillRoundedRectangle (bar.withY (top).withHeight (h), 2.0f);

            if (offset != 0 && bar.getWidth() >= 14.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.setFont (juce::Font (juce::FontOptions (9.0f)));
                g.drawText ((offset > 0 ? "+" : "") + juce::String (offset), bar.withHeight (12.0f).withY (offset > 0 ? bar.getBottom() - 12.0f : bar.getY()),
                            juce::Justification::centred, false);
            }
        }
    }
}

void RackRow::editGraphAt (juce::Point<int> p)
{
    auto* clip = getClip (false);
    const int mode = getGraphMode();

    if (clip == nullptr || mode == 0)
        return;

    auto area = getGraphArea();
    const int steps = getNumSteps();
    const int step = juce::jlimit (0, steps - 1, (p.x - area.getX()) * steps / juce::jmax (1, area.getWidth()));
    auto* n = noteAtStep (*clip, step, getGraphNote());

    if (n == nullptr)
        return;

    auto* um = &session.getEdit().getUndoManager();
    const float frac = juce::jlimit (0.0f, 1.0f, (area.getBottom() - p.y) / (float) juce::jmax (1, area.getHeight()));

    if (mode == 1)
    {
        n->setVelocity (juce::jlimit (1, 127, juce::roundToInt (frac * 127.0f)), um);
    }
    else
    {
        const int offset = juce::jlimit (-12, 12, juce::roundToInt ((frac - 0.5f) * 24.0f));
        const int newNote = juce::jlimit (0, 127, getChannelNote() + offset);

        // Hear each new pitch as you draw it
        if (newNote != n->getNoteNumber())
        {
            n->setNoteNumber (newNote, um);
            preview (newNote);
        }
    }

    repaint();
}
