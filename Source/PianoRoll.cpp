#include "PianoRoll.h"
#include "TheoryHelper.h"

namespace te = tracktion;

//==============================================================================
PianoRoll::PianoRoll (EditSession& sessionToUse, te::MidiClip& clipToEdit)
    : session (sessionToUse), clip (&clipToEdit)
{
    setWantsKeyboardFocus (true);

    for (auto* c : std::initializer_list<juce::Component*> { &gridBox, &quantiseButton, &hintLabel,
                                                             &keyLabel, &keyBox, &scaleBox, &chordLabel,
                                                             &chordRootBox, &chordTypeBox, &stampButton, &theory })
        addAndMakeVisible (c);

    // Grid
    gridBox.addItem ("Grid 1/4", 1);
    gridBox.addItem ("Grid 1/8", 2);
    gridBox.addItem ("Grid 1/16", 3);
    gridBox.addItem ("Grid 1/32", 4);
    gridBox.addItem ("Grid Off", 5);
    gridBox.setSelectedId (3, juce::dontSendNotification);
    gridBox.onChange = [this] { repaint(); };

    quantiseButton.onClick = [this] { quantise(); };

    addAndMakeVisible (theoryButton);
    theoryButton.setClickingTogglesState (false);
    theoryButton.onClick = [this] { setTheoryOpen (! theoryOpen); };

    hintLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    hintLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    hintLabel.setText ("Click to add, drag to move, drag the right edge to resize. Hold right-click and drag to erase.",
                       juce::dontSendNotification);

    // Theory controls
    for (int i = 0; i < 12; ++i)
    {
        keyBox.addItem (Theory::pitchName (i), i + 1);
        chordRootBox.addItem (Theory::pitchName (i), i + 1);
    }

    scaleBox.addItem ("No scale", 1);

    for (size_t i = 0; i < Theory::scales().size(); ++i)
        scaleBox.addItem (Theory::scales()[i].name, (int) i + 2);

    chordTypeBox.addItem ("No chord", 1);

    for (size_t i = 0; i < Theory::chordTypes().size(); ++i)
        chordTypeBox.addItem (Theory::chordTypes()[i].name, (int) i + 2);

    // Restore what this clip used last time
    keyBox.setSelectedId       ((int) clip->state.getProperty ("kngKey", 0) + 1, juce::dontSendNotification);
    scaleBox.setSelectedId     ((int) clip->state.getProperty ("kngScale", -1) + 2, juce::dontSendNotification);
    chordRootBox.setSelectedId ((int) clip->state.getProperty ("kngChordRoot", 0) + 1, juce::dontSendNotification);
    chordTypeBox.setSelectedId ((int) clip->state.getProperty ("kngChordType", -1) + 2, juce::dontSendNotification);
    stampButton.setToggleState ((bool) clip->state.getProperty ("kngStamp", false), juce::dontSendNotification);

    for (auto* box : { &keyBox, &scaleBox, &chordRootBox, &chordTypeBox })
        box->onChange = [this] { theoryChanged(); };

    stampButton.onClick = [this] { clip->state.setProperty ("kngStamp", stampButton.getToggleState(), nullptr); };

    viewBeat = getClipStartBeat();

    // Centre on the notes if there are any
    auto& seq = clip->getSequence();

    if (seq.getNumNotes() > 0)
        topNote = juce::jlimit (12, 127, seq.getNoteNumberRange().getEnd() + 6);

    rebuildChordButtons();
    updateTheoryPanel();

    setTheoryOpen ((bool) clip->state.getProperty ("kngTheoryOpen", false));
    startTimerHz (30);
    setSize (1240, 700);
}

PianoRoll::~PianoRoll()
{
    stopTimer();
}

//==============================================================================
// Theory

int PianoRoll::getKeyRoot() const      { return juce::jmax (0, keyBox.getSelectedId() - 1); }
int PianoRoll::getScaleIndex() const   { return scaleBox.getSelectedId() - 2; }
int PianoRoll::getChordRoot() const    { return juce::jmax (0, chordRootBox.getSelectedId() - 1); }
int PianoRoll::getChordType() const    { return chordTypeBox.getSelectedId() - 2; }

void PianoRoll::theoryChanged()
{
    clip->state.setProperty ("kngKey", getKeyRoot(), nullptr);
    clip->state.setProperty ("kngScale", getScaleIndex(), nullptr);
    clip->state.setProperty ("kngChordRoot", getChordRoot(), nullptr);
    clip->state.setProperty ("kngChordType", getChordType(), nullptr);

    // Only rebuild the chord buttons when the key changes
    if (getKeyRoot() != builtKey || getScaleIndex() != builtScale)
        rebuildChordButtons();

    for (int i = 0; i < chordButtons.size(); ++i)
        chordButtons[i]->setToggleState ((bool) chordButtons[i]->getProperties()["root"].equals (getChordRoot())
                                         && (bool) chordButtons[i]->getProperties()["type"].equals (getChordType()),
                                         juce::dontSendNotification);

    updateTheoryPanel();
    repaint();
}

void PianoRoll::rebuildChordButtons()
{
    chordButtons.clear();
    builtKey = getKeyRoot();
    builtScale = getScaleIndex();

    if (builtScale >= 0)
    {
        for (auto& dc : Theory::diatonicChords (builtKey, Theory::scales()[(size_t) builtScale]))
        {
            auto* b = chordButtons.add (new juce::TextButton (dc.numeral + " " + Theory::chordName (dc.root, Theory::chordTypes()[(size_t) dc.type])));
            b->getProperties().set ("root", dc.root);
            b->getProperties().set ("type", dc.type);
            b->setClickingTogglesState (false);
            b->setToggleState (dc.root == getChordRoot() && dc.type == getChordType(), juce::dontSendNotification);

            const int root = dc.root, type = dc.type;
            b->onClick = [this, root, type]
            {
                chordRootBox.setSelectedId (root + 1, juce::dontSendNotification);
                chordTypeBox.setSelectedId (type + 2, juce::dontSendNotification);
                theoryChanged();
            };

            addAndMakeVisible (b);
        }
    }

    resized();
}

void PianoRoll::updateTheoryPanel()
{
    theory.update (getKeyRoot(), getScaleIndex(), getChordRoot(), getChordType(), hoverNote);
}

//==============================================================================
// Geometry

juce::Rectangle<int> PianoRoll::getTheoryArea() const
{
    auto r = getLocalBounds().withTrimmedTop (toolbarHeight);
    return r.removeFromRight (theoryWidth);
}

juce::Rectangle<int> PianoRoll::getKeyboardArea() const
{
    auto r = getLocalBounds().withTrimmedTop (toolbarHeight + rulerHeight)
                             .withTrimmedBottom (velocityHeight)
                             .withTrimmedRight (theoryWidth);
    return r.removeFromLeft (keyboardWidth);
}

juce::Rectangle<int> PianoRoll::getGridArea() const
{
    return getLocalBounds().withTrimmedTop (toolbarHeight + rulerHeight)
                           .withTrimmedBottom (velocityHeight)
                           .withTrimmedLeft (keyboardWidth)
                           .withTrimmedRight (theoryWidth);
}

juce::Rectangle<int> PianoRoll::getRulerArea() const
{
    return { keyboardWidth, toolbarHeight, juce::jmax (0, getWidth() - keyboardWidth - theoryWidth), rulerHeight };
}

juce::Rectangle<int> PianoRoll::getVelocityArea() const
{
    auto r = getLocalBounds().withTrimmedTop (toolbarHeight).withTrimmedRight (theoryWidth);
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
// Grid

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
// Clip

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
// Notes

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

void PianoRoll::previewNotes (const juce::Array<int>& notes, int velocity)
{
    auto* at = dynamic_cast<te::AudioTrack*> (clip->getClipTrack());

    if (at == nullptr || notes.isEmpty())
        return;

    // The 5th argument (forceNote) must be true or the engine skips guide notes
    for (int i = 0; i < notes.size(); ++i)
        at->playGuideNote (notes[i], te::MidiChannel (1), velocity, i == 0, true, true);

    lastPreviewNote = notes.getLast();
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

void PianoRoll::eraseAt (juce::Point<int> p)
{
    bool edge = false;

    if (auto* n = findNoteAt (p, edge))
    {
        selected.removeFirstMatchingValue (n);
        clip->getSequence().removeNote (*n, um());
        repaint();
    }
}

//==============================================================================
void PianoRoll::timerCallback()
{
    // Clip was deleted; the dock clears this panel
    if (clip->getParent() == nullptr)
        return;

    cleanSelection();
    repaint();
}

void PianoRoll::resized()
{
    auto tb = getLocalBounds().removeFromTop (toolbarHeight).reduced (6, 4);
    auto row1 = tb.removeFromTop (28);

    gridBox.setBounds (row1.removeFromLeft (110));
    row1.removeFromLeft (6);
    quantiseButton.setBounds (row1.removeFromLeft (110));
    row1.removeFromLeft (6);
    theoryButton.setBounds (row1.removeFromLeft (80));
    row1.removeFromLeft (12);
    hintLabel.setBounds (row1);

    // Theory controls only show while Theory is on
    for (auto* c : std::initializer_list<juce::Component*> { &keyLabel, &keyBox, &scaleBox, &chordLabel,
                                                             &chordRootBox, &chordTypeBox, &stampButton, &theory })
        c->setVisible (theoryOpen);

    for (auto* b : chordButtons)
        b->setVisible (theoryOpen);

    if (theoryOpen)
    {
        tb.removeFromTop (4);
        auto row2 = tb.removeFromTop (28);

        keyLabel.setBounds (row2.removeFromLeft (32));
        keyBox.setBounds (row2.removeFromLeft (60));
        row2.removeFromLeft (4);
        scaleBox.setBounds (row2.removeFromLeft (150));
        row2.removeFromLeft (14);
        chordLabel.setBounds (row2.removeFromLeft (44));
        chordRootBox.setBounds (row2.removeFromLeft (60));
        row2.removeFromLeft (4);
        chordTypeBox.setBounds (row2.removeFromLeft (120));
        row2.removeFromLeft (8);
        stampButton.setBounds (row2.removeFromLeft (120));
        row2.removeFromLeft (8);

        for (auto* b : chordButtons)
        {
            b->setBounds (row2.removeFromLeft (76));
            row2.removeFromLeft (3);
        }

        theory.setBounds (getTheoryArea());
    }
}

void PianoRoll::setTheoryOpen (bool shouldBeOpen)
{
    theoryOpen = shouldBeOpen;
    clip->state.setProperty ("kngTheoryOpen", theoryOpen, nullptr);
    theoryButton.setToggleState (theoryOpen, juce::dontSendNotification);

    toolbarHeight = theoryOpen ? 70 : 38;
    theoryWidth   = theoryOpen ? 270 : 0;

    resized();
    repaint();
}

//==============================================================================
void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    bool edge = false;

    if (getGridArea().contains (p) && findNoteAt (p, edge) != nullptr)
        setMouseCursor (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);

    // Teach what's under the mouse
    const int newHover = (getGridArea().contains (p) || getKeyboardArea().contains (p)) ? yToNote (p.y) : -1;

    if (newHover != hoverNote)
    {
        hoverNote = newHover;
        updateTheoryPanel();
        repaint();
    }
}

void PianoRoll::mouseExit (const juce::MouseEvent&)
{
    if (hoverNote != -1)
    {
        hoverNote = -1;
        updateTheoryPanel();
        repaint();
    }
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    cleanSelection();

    const auto p = e.getPosition();

    // Ruler moves the playhead
    if (getRulerArea().contains (p))
    {
        const double beat = juce::jmax (getClipStartBeat(), xToBeat (p.x));
        session.getTransport().setPosition (clip->getTimeOfContentBeat (te::BeatPosition::fromBeats (beat)));
        return;
    }

    // Keyboard previews notes
    if (getKeyboardArea().contains (p))
    {
        previewNotes ({ yToNote (p.y) }, 100);
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

    // Hold right-click and drag to erase
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        session.beginEdit();
        erasing = true;
        eraseAt (p);
        return;
    }

    bool edge = false;
    auto* note = findNoteAt (p, edge);

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
        previewNotes ({ note->getNoteNumber() }, note->getVelocity());
    }
    else
    {
        // Add a note (or a whole chord) at the start of the grid cell you clicked
        selected.clear();

        const double start = juce::jmax (getClipStartBeat(), snapFloor (dragStartBeat));
        juce::Array<int> pitches { dragStartNote };

        if (stampButton.getToggleState() && getChordType() >= 0)
        {
            pitches.clear();

            for (int interval : Theory::chordTypes()[(size_t) getChordType()].intervals)
                if (dragStartNote + interval <= 127)
                    pitches.add (dragStartNote + interval);
        }

        for (int pitch : pitches)
        {
            auto* n = clip->getSequence().addNote (pitch,
                                                   te::BeatPosition::fromBeats (start),
                                                   te::BeatDuration::fromBeats (lastLength),
                                                   100, 0, um());

            if (n != nullptr)
            {
                selected.add (n);
                origs.push_back ({ n, start, lastLength, pitch });
            }
        }

        if (! origs.empty())
        {
            dragMode = DragMode::resize;
            ensureClipCovers (start + lastLength);
            previewNotes (pitches, 100);
        }
    }

    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    if (erasing)
    {
        eraseAt (e.getPosition());
        return;
    }

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
            previewNotes ({ pitch }, origs.front().note->getVelocity());
    }

    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (erasing || dragMode != DragMode::none)
    {
        // Push the change to every linked copy on the timeline
        if (clip->isLinked())
            clip->updateLinkedClips();

        session.save();
    }

    erasing = false;
    dragMode = DragMode::none;
    origs.clear();
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const float d = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;

    // Ctrl + scroll over the keys makes the rows taller or shorter
    if (e.mods.isCommandDown() && getKeyboardArea().contains (e.getPosition()))
    {
        const int noteAtMouse = yToNote (e.y);
        noteHeight = juce::jlimit (10, 40, noteHeight + (d > 0.0f ? 2 : -2));
        topNote = juce::jlimit (12, 127, noteAtMouse + (e.y - getGridArea().getY()) / noteHeight);
        repaint();
        return;
    }

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
    : DocumentWindow ("Piano Roll", juce::Colour (0xff1b1d20), DocumentWindow::allButtons),
      clip (&c)
{
    if (auto* t = c.getTrack())
        setName ("Piano Roll - " + t->getName());

    setUsingNativeTitleBar (true);
    setContentOwned (new PianoRoll (session, c), true);
    setResizable (true, false);
    centreWithSize (1240, 700);
    setVisible (true);
}
