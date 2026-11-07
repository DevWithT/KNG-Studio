#include "StepSequencer.h"

namespace te = tracktion;

static const juce::String padKeys = "1234QWERASDFZXCV";
static constexpr double stepBeats = 0.25;   // a 16th note

//==============================================================================
StepSequencer::StepSequencer (EditSession& sessionToUse, te::Track& trackToUse)
    : session (sessionToUse), track (&trackToUse)
{
    setWantsKeyboardFocus (true);

    for (auto* c : std::initializer_list<juce::Component*> { &newButton, &lengthBox, &loopButton, &clearButton,
                                                             &swingLabel, &swingSlider, &hintLabel })
        addAndMakeVisible (c);

    lengthBox.addItem ("1 bar", 1);
    lengthBox.addItem ("2 bars", 2);
    lengthBox.addItem ("4 bars", 4);
    lengthBox.setSelectedId (1, juce::dontSendNotification);
    lengthBox.onChange = [this] { setPatternBars (lengthBox.getSelectedId()); };

    swingSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    swingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 22);
    swingSlider.setRange (0.0, 100.0, 1.0);
    swingSlider.setTextValueSuffix ("%");
    swingSlider.setDoubleClickReturnValue (true, 0.0);
    swingSlider.onDragStart   = [this] { session.beginEdit(); };
    swingSlider.onValueChange = [this] { applySwing (swingSlider.getValue()); };
    swingSlider.onDragEnd     = [this] { session.save(); };

    newButton.onClick   = [this] { newPattern(); };
    loopButton.onClick  = [this] { loopPattern(); };
    clearButton.onClick = [this] { clearPattern(); };

    hintLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    hintLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    hintLabel.setText ("Click to add, drag to paint, right-drag to erase, scroll a hit for velocity",
                       juce::dontSendNotification);

    timerCallback();
    startTimerHz (20);
    setSize (920, 600);
}

StepSequencer::~StepSequencer()
{
    stopTimer();
}

int StepSequencer::getTrackIndex()
{
    for (int i = 0; i < session.getNumTracks(); ++i)
        if (session.getTrack (i) == track.get())
            return i;

    return -1;
}

juce::UndoManager* StepSequencer::um()
{
    return &session.getEdit().getUndoManager();
}

//==============================================================================
te::MidiClip* StepSequencer::findPatternClip()
{
    // 1. A drum clip you've selected on this track
    if (auto* selected = dynamic_cast<te::MidiClip*> (session.getSelectedClip()))
        if (selected->getTrack() == track.get() && selected->getParent() != nullptr)
            return selected;

    // 2. The pattern you were last working on
    if (clip != nullptr && clip->getParent() != nullptr)
        return clip.get();

    // 3. Whatever pattern sits under the playhead
    if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track.get()))
    {
        const auto pos = session.getTransport().getPosition();

        for (auto* c : clipTrack->getClips())
            if (auto* mc = dynamic_cast<te::MidiClip*> (c))
                if (pos >= mc->getPosition().getStart() && pos < mc->getPosition().getEnd())
                    return mc;
    }

    return nullptr;
}

void StepSequencer::syncControls()
{
    const bool hasClip = clip != nullptr;

    loopButton.setEnabled (hasClip);
    clearButton.setEnabled (hasClip);
    swingSlider.setEnabled (hasClip);

    if (! hasClip)
        return;

    const int bars = juce::roundToInt (getClipLengthBeats() / 4.0);
    lengthBox.setSelectedId (bars >= 4 ? 4 : bars >= 2 ? 2 : 1, juce::dontSendNotification);
    swingSlider.setValue ((double) clip->state.getProperty ("kngSwing", 0.0), juce::dontSendNotification);
}

void StepSequencer::timerCallback()
{
    auto* found = findPatternClip();

    if (found != clip.get())
    {
        clip = found;
        syncControls();
    }

    repaint();
}

//==============================================================================
double StepSequencer::getClipStartBeat() const
{
    return clip->getContentBeatAtTime (clip->getPosition().getStart()).inBeats();
}

double StepSequencer::getClipLengthBeats() const
{
    return clip->getContentBeatAtTime (clip->getPosition().getEnd()).inBeats() - getClipStartBeat();
}

int StepSequencer::getNumSteps() const
{
    if (clip == nullptr)
        return 16;

    return juce::jlimit (1, 64, juce::roundToInt (getClipLengthBeats() / stepBeats));
}

double StepSequencer::getSwingDelay() const
{
    // Up to half a 16th late on the off-steps
    return (double) clip->state.getProperty ("kngSwing", 0.0) / 100.0 * (stepBeats * 0.5);
}

double StepSequencer::stepToBeat (int step) const
{
    return step * stepBeats + ((step % 2) == 1 ? getSwingDelay() : 0.0);
}

int StepSequencer::beatToStep (double beatFromClipStart) const
{
    // Swing is less than half a step, so flooring finds the right step either way
    return (int) std::floor ((beatFromClipStart + 0.001) / stepBeats);
}

//==============================================================================
juce::Rectangle<int> StepSequencer::getGridArea() const
{
    return getLocalBounds().withTrimmedTop (toolbarHeight).reduced (8).withTrimmedLeft (labelWidth);
}

juce::Rectangle<int> StepSequencer::getCellBounds (int row, int step) const
{
    auto grid = getGridArea();
    const int steps = getNumSteps();

    const int x1 = grid.getX() + step * grid.getWidth() / steps;
    const int x2 = grid.getX() + (step + 1) * grid.getWidth() / steps;
    const int y1 = grid.getY() + row * grid.getHeight() / 16;
    const int y2 = grid.getY() + (row + 1) * grid.getHeight() / 16;

    return { x1, y1, x2 - x1, y2 - y1 };
}

bool StepSequencer::cellAt (juce::Point<int> p, int& row, int& step) const
{
    auto grid = getGridArea();

    if (! grid.contains (p))
        return false;

    step = juce::jlimit (0, getNumSteps() - 1, (p.x - grid.getX()) * getNumSteps() / grid.getWidth());
    row  = juce::jlimit (0, 15, (p.y - grid.getY()) * 16 / grid.getHeight());
    return true;
}

te::MidiNote* StepSequencer::findNote (int row, int step)
{
    const int note = EditSession::getPadNote (row);
    const double start = getClipStartBeat();

    for (auto* n : clip->getSequence().getNotes())
        if (n->getNoteNumber() == note && beatToStep (n->getStartBeat().inBeats() - start) == step)
            return n;

    return nullptr;
}

void StepSequencer::setStep (int row, int step, bool on)
{
    auto* existing = findNote (row, step);

    if (on && existing == nullptr)
    {
        clip->getSequence().addNote (EditSession::getPadNote (row),
                                     te::BeatPosition::fromBeats (getClipStartBeat() + stepToBeat (step)),
                                     te::BeatDuration::fromBeats (stepBeats),
                                     100, 0, um());
    }
    else if (! on && existing != nullptr)
    {
        clip->getSequence().removeNote (*existing, um());
    }
}

//==============================================================================
void StepSequencer::newPattern()
{
    const int idx = getTrackIndex();

    if (idx < 0)
        return;

    const double pos = session.getTransport().getPosition().inSeconds();

    if (auto* mc = session.createPatternClip (idx, pos, lengthBox.getSelectedId()))
    {
        clip = mc;
        syncControls();
    }
}

void StepSequencer::setPatternBars (int bars)
{
    if (clip == nullptr)
        return;

    session.beginEdit();
    clip->setEnd (clip->getTimeOfContentBeat (te::BeatPosition::fromBeats (getClipStartBeat() + bars * 4.0)), true);
    session.save();
}

void StepSequencer::loopPattern()
{
    if (clip == nullptr)
        return;

    const auto pos = clip->getPosition();

    session.setLoopRange (pos.getStart().inSeconds(), pos.getEnd().inSeconds());
    session.setLooping (true);
    session.getTransport().setPosition (pos.getStart());

    if (! session.getTransport().isPlaying())
        session.play();
}

void StepSequencer::clearPattern()
{
    if (clip == nullptr)
        return;

    session.beginEdit();
    clip->getSequence().clear (um());
    session.save();
}

void StepSequencer::applySwing (double newPercent)
{
    if (clip == nullptr)
        return;

    const double start = getClipStartBeat();

    // Find each hit's step with the old swing, then place it with the new swing
    std::vector<std::pair<te::MidiNote*, int>> placed;

    for (auto* n : clip->getSequence().getNotes())
        placed.push_back ({ n, beatToStep (n->getStartBeat().inBeats() - start) });

    clip->state.setProperty ("kngSwing", newPercent, um());

    for (auto& [n, step] : placed)
        n->setStartAndLength (te::BeatPosition::fromBeats (start + stepToBeat (step)), n->getLengthBeats(), um());
}

//==============================================================================
void StepSequencer::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));

    g.setColour (juce::Colour (0xff2b2f33));
    g.fillRect (getLocalBounds().removeFromTop (toolbarHeight));

    if (clip == nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawText ("No pattern yet. Click New Pattern to start one at the playhead.",
                    getLocalBounds().withTrimmedTop (toolbarHeight), juce::Justification::centred);
        return;
    }

    const int idx = getTrackIndex();
    const int steps = getNumSteps();
    auto grid = getGridArea();

    // Which step is playing
    const double playBeat = clip->getContentBeatAtTime (session.getTransport().getPosition()).inBeats() - getClipStartBeat();
    const int playStep = (session.getTransport().isPlaying() && playBeat >= 0.0 && playBeat < steps * stepBeats)
                           ? (int) (playBeat / stepBeats) : -1;

    // Velocity of every hit, by pad and step
    int velocity[16][64] = {};
    const double start = getClipStartBeat();

    for (auto* n : clip->getSequence().getNotes())
    {
        const int row  = n->getNoteNumber() - EditSession::getPadNote (0);
        const int step = beatToStep (n->getStartBeat().inBeats() - start);

        if (juce::isPositiveAndBelow (row, 16) && juce::isPositiveAndBelow (step, steps))
            velocity[row][step] = n->getVelocity();
    }

    g.setFont (juce::Font (juce::FontOptions (12.0f)));

    for (int row = 0; row < 16; ++row)
    {
        // Row label
        auto first = getCellBounds (row, 0);
        juce::Rectangle<int> label (grid.getX() - labelWidth, first.getY(), labelWidth - 6, first.getHeight());

        g.setColour (juce::Colour (row % 2 == 0 ? 0xff2b2f33 : 0xff272a2e));
        g.fillRect (label);

        const auto name = session.getPadName (idx, row);
        g.setColour (juce::Colours::white.withAlpha (name.isNotEmpty() ? 0.9f : 0.35f));
        g.drawFittedText (padKeys.substring (row, row + 1) + "  " + (name.isNotEmpty() ? name : "Pad " + juce::String (row + 1)),
                          label.reduced (6, 0), juce::Justification::centredLeft, 1);

        // Steps
        for (int step = 0; step < steps; ++step)
        {
            auto cell = getCellBounds (row, step).reduced (1);

            g.setColour ((step / 4) % 2 == 0 ? juce::Colour (0xff2e3338) : juce::Colour (0xff25292d));
            g.fillRect (cell);

            if (step == playStep)
            {
                g.setColour (juce::Colours::white.withAlpha (0.08f));
                g.fillRect (cell);
            }

            if (velocity[row][step] > 0)
            {
                const float v = velocity[row][step] / 127.0f;
                g.setColour (juce::Colour (0xff3a8ee6).withAlpha (0.35f + 0.65f * v));
                g.fillRoundedRectangle (cell.reduced (2).toFloat(), 3.0f);
            }
        }
    }

    // Bar lines
    g.setColour (juce::Colours::white.withAlpha (0.25f));

    for (int step = 16; step < steps; step += 16)
        g.drawVerticalLine (getCellBounds (0, step).getX(), (float) grid.getY(), (float) grid.getBottom());
}

void StepSequencer::resized()
{
    auto r = getLocalBounds().removeFromTop (toolbarHeight).reduced (6);

    newButton.setBounds (r.removeFromLeft (100));
    r.removeFromLeft (6);
    lengthBox.setBounds (r.removeFromLeft (90));
    r.removeFromLeft (6);
    loopButton.setBounds (r.removeFromLeft (110));
    r.removeFromLeft (6);
    clearButton.setBounds (r.removeFromLeft (70));
    r.removeFromLeft (12);
    swingLabel.setBounds (r.removeFromLeft (45));
    swingSlider.setBounds (r.removeFromLeft (170));
    r.removeFromLeft (10);
    hintLabel.setBounds (r);
}

//==============================================================================
void StepSequencer::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    if (clip == nullptr)
        return;

    auto grid = getGridArea();
    const auto p = e.getPosition();

    // Clicking a row's name plays that pad
    if (p.x < grid.getX() && p.y >= grid.getY() && p.y < grid.getBottom())
    {
        const int row = juce::jlimit (0, 15, (p.y - grid.getY()) * 16 / grid.getHeight());
        session.playPad (getTrackIndex(), row, 110);
        return;
    }

    int row = 0, step = 0;

    if (! cellAt (p, row, step))
        return;

    // Left on an empty step paints hits, anything else erases
    paintOn = ! e.mods.isPopupMenu() && findNote (row, step) == nullptr;

    session.beginEdit();
    painting = true;
    lastRow = row;
    lastStep = step;

    setStep (row, step, paintOn);

    if (paintOn)
        session.playPad (getTrackIndex(), row, 100);

    repaint();
}

void StepSequencer::mouseDrag (const juce::MouseEvent& e)
{
    if (! painting || clip == nullptr)
        return;

    int row = 0, step = 0;

    if (cellAt (e.getPosition(), row, step) && (row != lastRow || step != lastStep))
    {
        lastRow = row;
        lastStep = step;
        setStep (row, step, paintOn);
        repaint();
    }
}

void StepSequencer::mouseUp (const juce::MouseEvent&)
{
    if (painting)
        session.save();

    painting = false;
}

void StepSequencer::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int row = 0, step = 0;

    if (clip != nullptr && cellAt (e.getPosition(), row, step))
    {
        if (auto* n = findNote (row, step))
        {
            n->setVelocity (juce::jlimit (1, 127, n->getVelocity() + (wheel.deltaY > 0.0f ? 8 : -8)), um());
            repaint();
            return;
        }
    }

    juce::Component::mouseWheelMove (e, wheel);
}

bool StepSequencer::keyPressed (const juce::KeyPress& key)
{
    if (key.getModifiers().isAnyModifierKeyDown())
        return false;

    const int pad = padKeys.indexOfChar (juce::CharacterFunctions::toUpperCase (key.getTextCharacter()));

    if (pad >= 0)
    {
        session.playPad (getTrackIndex(), pad, 110);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (session.getTransport().isPlaying())
            session.stop();
        else
            session.play();

        return true;
    }

    return false;
}
