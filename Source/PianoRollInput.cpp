#include "PianoRoll.h"
#include "TheoryHelper.h"
#include "KngSamplerPlugin.h"
#include "KngSynthPlugin.h"

namespace te = tracktion;

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

    if (code == 'G' || code == 'g')
    {
        toggleSlideOnSelected();
        return true;
    }

    // Note tools: H humanizes, L makes every note legato, A turns chords into a rising 1/16 arpeggio
    if (code == 'H' || code == 'h')
    {
        selected.clear();
        session.humanizeClip (*clip, 0.04, 12);
        repaint();
        return true;
    }

    if (code == 'L' || code == 'l')
    {
        selected.clear();
        session.setClipNoteLength (*clip, 0.0);
        repaint();
        return true;
    }

    if (code == 'A' || code == 'a')
    {
        selected.clear();
        session.arpeggiateClip (*clip, 0, 0.25);
        repaint();
        return true;
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
void PianoRoll::showToolsMenu()
{
    juce::Component::SafePointer<PianoRoll> safe (this);

    // Each tool runs on this clip, clears the selection (arpeggiate replaces notes) and redraws
    auto run = [safe] (std::function<void (EditSession&, te::MidiClip&)> tool)
    {
        return [safe, tool]
        {
            if (safe == nullptr || safe->clip->getParent() == nullptr)
                return;

            safe->selected.clear();
            tool (safe->session, *safe->clip);
            safe->repaint();
        };
    };

    juce::PopupMenu menu, humanize, arp, length;

    humanize.addItem ("Light",  run ([] (EditSession& s, te::MidiClip& c) { s.humanizeClip (c, 0.02, 6); }));
    humanize.addItem ("Medium (H)", run ([] (EditSession& s, te::MidiClip& c) { s.humanizeClip (c, 0.04, 12); }));
    humanize.addItem ("Heavy",  run ([] (EditSession& s, te::MidiClip& c) { s.humanizeClip (c, 0.08, 20); }));
    menu.addSubMenu ("Humanize", humanize);

    const char* dirNames[] = { "Up", "Down", "Up-Down" };
    const double rates[] = { 0.25, 0.5, 0.125 };
    const char* rateNames[] = { "1/16", "1/8", "1/32" };

    for (int r = 0; r < 3; ++r)
    {
        for (int d = 0; d < 3; ++d)
        {
            const double rate = rates[r];
            const auto label = juce::String (dirNames[d]) + " " + rateNames[r] + (r == 0 && d == 0 ? "  (A)" : "");
            arp.addItem (label, run ([d, rate] (EditSession& s, te::MidiClip& c) { s.arpeggiateClip (c, d, rate); }));
        }

        if (r < 2)
            arp.addSeparator();
    }

    menu.addSubMenu ("Arpeggiate Chords", arp);

    const double lengths[] = { 0.25, 0.5, 1.0, 2.0 };
    const char* lengthNames[] = { "1/16", "1/8", "1/4", "1/2" };

    for (int k = 0; k < 4; ++k)
    {
        const double beats = lengths[k];
        length.addItem (lengthNames[k], run ([beats] (EditSession& s, te::MidiClip& c) { s.setClipNoteLength (c, beats); }));
    }

    length.addSeparator();
    length.addItem ("Legato (L)", run ([] (EditSession& s, te::MidiClip& c) { s.setClipNoteLength (c, 0.0); }));
    menu.addSubMenu ("Note Length", length);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&toolsButton));
}
