#include "PianoRoll.h"

namespace te = tracktion;

static bool isBlackKey (int note)
{
    const int n = note % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

//==============================================================================
PianoRoll::PianoRoll (EditSession& sessionToUse, te::MidiClip& clipToEdit)
    : session (sessionToUse), clip (&clipToEdit)
{
    setWantsKeyboardFocus (true);

    addAndMakeVisible (gridBox);
    gridBox.addItem ("Grid 1/4", 1);
    gridBox.addItem ("Grid 1/8", 2);
    gridBox.addItem ("Grid 1/16", 3);
    gridBox.addItem ("Grid 1/32", 4);
    gridBox.addItem ("Grid Off", 5);
    gridBox.setSelectedId (3, juce::dontSendNotification);
    gridBox.onChange = [this] { repaint(); };

    addAndMakeVisible (quantiseButton);
    quantiseButton.onClick = [this] { quantise(); };

    addAndMakeVisible (hintLabel);
    hintLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    hintLabel.setText ("Click to add. Drag to move, drag right edge to resize. Right-click or Del deletes. Alt = no snap.",
                       juce::dontSendNotification);

    viewBeat = getClipStartBeat();

    // Centre on the notes if there are any
    auto& seq = clip->getSequence();

    if (seq.getNumNotes() > 0)
        topNote = juce::jlimit (12, 127, seq.getNoteNumberRange().getEnd() + 6);

    startTimerHz (30);
    setSize (960, 600);
}

PianoRoll::~PianoRoll()
{
    stopTimer();
}

//==============================================================================
juce::Rectangle<int> PianoRoll::getKeyboardArea() const
{
    auto r = getLocalBounds().withTrimmedTop (toolbarHeight).withTrimmedBottom (velocityHeight);
    return r.removeFromLeft (keyboardWidth);
}

juce::Rectangle<int> PianoRoll::getGridArea() const
{
    return getLocalBounds().withTrimmedTop (toolbarHeight)
                           .withTrimmedBottom (velocityHeight)
                           .withTrimmedLeft (keyboardWidth);
}

juce::Rectangle<int> PianoRoll::getVelocityArea() const
{
    auto r = getLocalBounds();
    return r.removeFromBottom (velocityHeight).withTrimmedLeft (keyboardWidth);
}

double PianoRoll::xToBeat (int x) const
{
    return viewBeat + (x - getGridArea().getX()) / pxPerBeat;
}

int PianoRoll::beatToX (double beat) const
{
    return getGridArea().getX() + juce::roundToInt ((beat - viewBeat) * pxPerBeat);
}

int PianoRoll::noteToY (int note) const
{
    return getGridArea().getY() + (topNote - note) * noteHeight;
}

int PianoRoll::yToNote (int y) const
{
    const int rows = (int) std::floor ((y - getGridArea().getY()) / (double) noteHeight);
    return juce::jlimit (0, 127, topNote - rows);
}

juce::Rectangle<int> PianoRoll::getNoteRect (te::MidiNote& n) const
{
    const int x = beatToX (n.getStartBeat().inBeats());
    const int w = juce::jmax (3, juce::roundToInt (n.getLengthBeats().inBeats() * pxPerBeat));
    return { x, noteToY (n.getNoteNumber()) + 1, w, noteHeight - 2 };
}

//==============================================================================
double PianoRoll::getGridBeats() const
{
    switch (gridBox.getSelectedId())
    {
        case 1:  return 1.0;
        case 2:  return 0.5;
        case 3:  return 0.25;
        case 4:  return 0.125;
        default: return 0.0;
    }
}

double PianoRoll::snapBeat (double beat) const
{
    const double g = getGridBeats();
    return g > 0.0 ? std::round (beat / g) * g : beat;
}

double PianoRoll::snapFloor (double beat) const
{
    const double g = getGridBeats();
    return g > 0.0 ? std::floor (beat / g) * g : beat;
}

//==============================================================================
double PianoRoll::getClipStartBeat() const
{
    return clip->getContentBeatAtTime (clip->getPosition().getStart()).inBeats();
}

double PianoRoll::getClipEndBeat() const
{
    return clip->getContentBeatAtTime (clip->getPosition().getEnd()).inBeats();
}

void PianoRoll::ensureClipCovers (double endBeat)
{
    if (endBeat <= getClipEndBeat())
        return;

    // Grow the clip to the next whole bar
    const double newEndBeat = std::ceil (endBeat / 4.0) * 4.0;
    clip->setEnd (clip->getTimeOfContentBeat (te::BeatPosition::fromBeats (newEndBeat)), true);
}

juce::UndoManager* PianoRoll::um()
{
    return &session.getEdit().getUndoManager();
}

//==============================================================================
bool PianoRoll::isValid (te::MidiNote* n) const
{
    return n != nullptr && clip->getSequence().getNotes().contains (n);
}

void PianoRoll::cleanSelection()
{
    for (int i = selected.size(); --i >= 0;)
        if (! isValid (selected[i]))
            selected.remove (i);
}

te::MidiNote* PianoRoll::findNoteAt (juce::Point<int> p, bool& onRightEdge)
{
    onRightEdge = false;

    for (auto* n : clip->getSequence().getNotes())
    {
        auto r = getNoteRect (*n);

        if (r.contains (p))
        {
            onRightEdge = r.getWidth() > 10 && p.x >= r.getRight() - 6;
            return n;
        }
    }

    return nullptr;
}

void PianoRoll::previewNote (int note, int velocity)
{
    if (auto* at = dynamic_cast<te::AudioTrack*> (clip->getClipTrack()))
        at->playGuideNote (note, te::MidiChannel (1), velocity, true, false, true);

    lastPreviewNote = note;
}

void PianoRoll::applyVelocity (juce::Point<int> p)
{
    auto va = getVelocityArea();
    const int vel = juce::jlimit (1, 127, juce::roundToInt ((va.getBottom() - p.y) / (double) va.getHeight() * 127.0));

    // Nearest note start within a few pixels
    te::MidiNote* best = nullptr;
    int bestDist = 9;

    for (auto* n : clip->getSequence().getNotes())
    {
        const int d = std::abs (beatToX (n->getStartBeat().inBeats()) - p.x);

        if (d < bestDist)
        {
            best = n;
            bestDist = d;
        }
    }

    if (best == nullptr)
        return;

    if (selected.contains (best))
    {
        for (auto* n : selected)
            n->setVelocity (vel, um());
    }
    else
    {
        best->setVelocity (vel, um());
    }

    repaint();
}

void PianoRoll::quantise()
{
    const double g = getGridBeats();

    if (g <= 0.0)
        return;

    cleanSelection();
    session.beginEdit();

    auto notes = selected.isEmpty() ? clip->getSequence().getNotes() : selected;

    for (auto* n : notes)
    {
        const double start = std::round (n->getStartBeat().inBeats() / g) * g;
        n->setStartAndLength (te::BeatPosition::fromBeats (start), n->getLengthBeats(), um());
    }

    session.save();
    repaint();
}

void PianoRoll::deleteSelected()
{
    cleanSelection();

    if (selected.isEmpty())
        return;

    session.beginEdit();
    auto toRemove = selected;
    selected.clear();

    for (auto* n : toRemove)
        if (isValid (n))
            clip->getSequence().removeNote (*n, um());

    session.save();
    repaint();
}

//==============================================================================
void PianoRoll::timerCallback()
{
    // Clip was deleted, so hide the window
    if (clip->getParent() == nullptr)
    {
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
            w->setVisible (false);

        return;
    }

    cleanSelection();
    repaint();
}

void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));

    // Toolbar
    g.setColour (juce::Colour (0xff2b2f33));
    g.fillRect (getLocalBounds().removeFromTop (toolbarHeight));

    auto grid = getGridArea();
    const double startBeat = getClipStartBeat();
    const double endBeat   = getClipEndBeat();

    //==========================================================================
    {
        juce::Graphics::ScopedSaveState saveState (g);
        g.reduceClipRegion (grid);

        // Rows
        for (int note = topNote; note >= 0 && noteToY (note) < grid.getBottom(); --note)
        {
            const int y = noteToY (note);
            g.setColour (isBlackKey (note) ? juce::Colour (0xff262a2e) : juce::Colour (0xff2e3338));
            g.fillRect (grid.getX(), y, grid.getWidth(), noteHeight);

            if (note % 12 == 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.drawHorizontalLine (y + noteHeight - 1, (float) grid.getX(), (float) grid.getRight());
            }
        }

        // Shade outside the clip
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        const int cx1 = beatToX (startBeat);
        const int cx2 = beatToX (endBeat);

        if (cx1 > grid.getX())
            g.fillRect (grid.getX(), grid.getY(), cx1 - grid.getX(), grid.getHeight());

        if (cx2 < grid.getRight())
            g.fillRect (cx2, grid.getY(), grid.getRight() - cx2, grid.getHeight());

        // Grid lines
        const double step = getGridBeats() > 0.0 ? getGridBeats() : 0.25;
        const double visibleEnd = xToBeat (grid.getRight());

        for (int i = (int) std::floor (viewBeat / step); i * step <= visibleEnd; ++i)
        {
            const double b = i * step;
            const bool isBeat = std::abs (b - std::round (b)) < 1e-6;
            const bool isBar  = isBeat && ((int) std::round (b) % 4) == 0;

            if (step * pxPerBeat < 4.0 && ! isBeat)
                continue;

            g.setColour (juce::Colours::white.withAlpha (isBar ? 0.25f : isBeat ? 0.12f : 0.05f));
            g.drawVerticalLine (beatToX (b), (float) grid.getY(), (float) grid.getBottom());
        }

        // Notes
        for (auto* n : clip->getSequence().getNotes())
        {
            auto r = getNoteRect (*n);

            if (! r.intersects (grid))
                continue;

            const float vel = n->getVelocity() / 127.0f;
            const bool sel = selected.contains (n);

            g.setColour (juce::Colour (0xff3a8ee6).interpolatedWith (juce::Colour (0xffe6893a), vel));
            g.fillRoundedRectangle (r.toFloat(), 2.0f);

            g.setColour (sel ? juce::Colours::white : juce::Colours::black.withAlpha (0.6f));
            g.drawRoundedRectangle (r.toFloat(), 2.0f, sel ? 2.0f : 1.0f);
        }

        // Playhead
        const double playBeat = clip->getContentBeatAtTime (session.getTransport().getPosition()).inBeats();
        g.setColour (juce::Colours::yellow);
        g.fillRect (beatToX (playBeat), grid.getY(), 2, grid.getHeight());
    }

    //==========================================================================
    // Keyboard
    auto kb = getKeyboardArea();

    {
        juce::Graphics::ScopedSaveState saveState (g);
        g.reduceClipRegion (kb);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));

        for (int note = topNote; note >= 0 && noteToY (note) < kb.getBottom(); --note)
        {
            const int y = noteToY (note);
            const bool black = isBlackKey (note);

            g.setColour (black ? juce::Colour (0xff1b1b1b) : juce::Colour (0xffe8e8e8));
            g.fillRect (kb.getX(), y, kb.getWidth(), noteHeight);

            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawHorizontalLine (y + noteHeight - 1, (float) kb.getX(), (float) kb.getRight());

            if (note % 12 == 0)
            {
                g.setColour (juce::Colours::black);
                g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                            kb.getX() + 2, y, kb.getWidth() - 4, noteHeight, juce::Justification::centredRight);
            }
        }
    }

    //==========================================================================
    // Velocity lane
    auto va = getVelocityArea();
    g.setColour (juce::Colour (0xff181a1d));
    g.fillRect (va);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine (va.getY(), (float) va.getX(), (float) va.getRight());

    {
        juce::Graphics::ScopedSaveState saveState (g);
        g.reduceClipRegion (va);

        for (auto* n : clip->getSequence().getNotes())
        {
            const int x = beatToX (n->getStartBeat().inBeats());
            const int h = juce::roundToInt (n->getVelocity() / 127.0 * (va.getHeight() - 6));

            g.setColour (selected.contains (n) ? juce::Colours::white : juce::Colour (0xff3a8ee6));
            g.fillRect (x, va.getBottom() - h, 4, h);
        }
    }
}

void PianoRoll::resized()
{
    gridBox.setBounds (6, 5, 110, 24);
    quantiseButton.setBounds (122, 5, 110, 24);
    hintLabel.setBounds (240, 5, getWidth() - 246, 24);
}

//==============================================================================
void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    bool edge = false;

    if (getGridArea().contains (e.getPosition()) && findNoteAt (e.getPosition(), edge) != nullptr)
        setMouseCursor (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    cleanSelection();

    const auto p = e.getPosition();

    // Keyboard previews notes
    if (getKeyboardArea().contains (p))
    {
        previewNote (yToNote (p.y), 100);
        return;
    }

    // Velocity lane
    if (getVelocityArea().contains (p))
    {
        session.beginEdit();
        dragMode = DragMode::velocity;
        applyVelocity (p);
        return;
    }

    if (! getGridArea().contains (p))
        return;

    bool edge = false;
    auto* note = findNoteAt (p, edge);

    // Right-click deletes
    if (e.mods.isPopupMenu())
    {
        if (note != nullptr)
        {
            if (! selected.contains (note))
            {
                selected.clear();
                selected.add (note);
            }

            deleteSelected();
        }

        return;
    }

    session.beginEdit();
    dragStartBeat = xToBeat (p.x);
    dragStartNote = yToNote (p.y);
    origs.clear();

    if (note != nullptr)
    {
        if (e.mods.isShiftDown())
        {
            if (selected.contains (note))
                selected.removeFirstMatchingValue (note);
            else
                selected.add (note);
        }
        else if (! selected.contains (note))
        {
            selected.clear();
            selected.add (note);
        }

        dragMode = edge ? DragMode::resize : DragMode::move;

        for (auto* n : selected)
            origs.push_back ({ n, n->getStartBeat().inBeats(), n->getLengthBeats().inBeats(), n->getNoteNumber() });

        lastLength = note->getLengthBeats().inBeats();
        previewNote (note->getNoteNumber(), note->getVelocity());
    }
    else
    {
        // Add a new note at the start of the grid cell you clicked
        selected.clear();

        const double start = juce::jmax (getClipStartBeat(), snapFloor (dragStartBeat));
        auto* n = clip->getSequence().addNote (dragStartNote,
                                               te::BeatPosition::fromBeats (start),
                                               te::BeatDuration::fromBeats (lastLength),
                                               100, 0, um());

        if (n != nullptr)
        {
            selected.add (n);
            origs.push_back ({ n, start, lastLength, dragStartNote });
            dragMode = DragMode::resize;
            ensureClipCovers (start + lastLength);
            previewNote (dragStartNote, 100);
        }
    }

    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::none)
        return;

    if (dragMode == DragMode::velocity)
    {
        applyVelocity (e.getPosition());
        return;
    }

    const bool snap = ! e.mods.isAltDown();
    const double delta = xToBeat (e.x) - dragStartBeat;
    const int noteDelta = yToNote (e.y) - dragStartNote;
    const double minLength = getGridBeats() > 0.0 ? getGridBeats() : 0.0625;

    for (auto& o : origs)
    {
        if (! isValid (o.note))
            continue;

        if (dragMode == DragMode::move)
        {
            double start = o.start + delta;

            if (snap)
                start = snapBeat (start);

            start = juce::jmax (getClipStartBeat(), start);

            o.note->setStartAndLength (te::BeatPosition::fromBeats (start), te::BeatDuration::fromBeats (o.length), um());
            o.note->setNoteNumber (juce::jlimit (0, 127, o.pitch + noteDelta), um());
            ensureClipCovers (start + o.length);
        }
        else if (dragMode == DragMode::resize)
        {
            double end = o.start + o.length + delta;

            if (snap)
                end = snapBeat (end);

            const double length = juce::jmax (minLength, end - o.start);
            o.note->setStartAndLength (te::BeatPosition::fromBeats (o.start), te::BeatDuration::fromBeats (length), um());
            lastLength = length;
            ensureClipCovers (o.start + length);
        }
    }

    // Hear the new pitch when dragging a note up or down
    if (dragMode == DragMode::move && ! origs.empty() && isValid (origs.front().note))
    {
        const int pitch = origs.front().note->getNoteNumber();

        if (pitch != lastPreviewNote)
            previewNote (pitch, origs.front().note->getVelocity());
    }

    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (dragMode != DragMode::none)
        session.save();

    dragMode = DragMode::none;
    origs.clear();
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const float d = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;

    if (e.mods.isCommandDown())
    {
        // Zoom around the mouse
        const double beatAtMouse = xToBeat (e.x);
        pxPerBeat = juce::jlimit (10.0, 400.0, pxPerBeat * (d > 0.0f ? 1.2 : 1.0 / 1.2));
        viewBeat = juce::jmax (0.0, beatAtMouse - (e.x - getGridArea().getX()) / pxPerBeat);
    }
    else if (e.mods.isShiftDown() || std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
    {
        const double visibleBeats = getGridArea().getWidth() / pxPerBeat;
        viewBeat = juce::jmax (0.0, viewBeat - d * visibleBeats * 0.3);
    }
    else
    {
        topNote = juce::jlimit (12, 127, topNote + (d > 0.0f ? 3 : -3));
    }

    repaint();
}

bool PianoRoll::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const int code  = key.getKeyCode();

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelected();
        return true;
    }

    if (mods.isCommandDown())
    {
        if (code == 'A' || code == 'a')
        {
            selected = clip->getSequence().getNotes();
            repaint();
            return true;
        }

        if (code == 'Z' || code == 'z')
        {
            selected.clear();

            if (mods.isShiftDown())
                session.redo();
            else
                session.undo();

            return true;
        }

        if (code == 'Y' || code == 'y')
        {
            selected.clear();
            session.redo();
            return true;
        }

        return false;
    }

    if (code == 'Q' || code == 'q')
    {
        quantise();
        return true;
    }

    if (code == juce::KeyPress::spaceKey)
    {
        if (session.getTransport().isPlaying())
            session.stop();
        else
            session.play();

        return true;
    }

    return false;
}

//==============================================================================
PianoRollWindow::PianoRollWindow (EditSession& session, te::MidiClip& c)
    : DocumentWindow ("Piano Roll", juce::Colour (0xff1e2124), DocumentWindow::allButtons),
      clip (&c)
{
    if (auto* t = c.getTrack())
        setName ("Piano Roll - " + t->getName());

    setUsingNativeTitleBar (true);
    setContentOwned (new PianoRoll (session, c), true);
    setResizable (true, false);
    centreWithSize (960, 600);
    setVisible (true);
}
