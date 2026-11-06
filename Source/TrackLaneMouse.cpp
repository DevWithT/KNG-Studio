#include "TrackLane.h"

namespace te = tracktion;

//==============================================================================
te::Clip* TrackLane::findClipAt (int x)
{
    for (auto& ct : clipThumbs)
    {
        auto pos = ct.clip->getPosition();
        const int x1 = timeToX (pos.getStart().inSeconds());
        const int x2 = timeToX (pos.getEnd().inSeconds());

        if (x >= x1 && x <= x2)
            return ct.clip.get();
    }

    return nullptr;
}

TrackLane::DragMode TrackLane::getModeAt (te::Clip& clip, juce::Point<int> p) const
{
    auto pos = clip.getPosition();
    const int x1 = timeToX (pos.getStart().inSeconds());
    const int x2 = timeToX (pos.getEnd().inSeconds());

    // Fade handles live along the top edge
    if (auto* acb = dynamic_cast<te::AudioClipBase*> (&clip))
    {
        if (p.y <= fadeHandleSize + 2)
        {
            const int xIn  = timeToX (pos.getStart().inSeconds() + acb->getFadeIn().inSeconds());
            const int xOut = timeToX (pos.getEnd().inSeconds() - acb->getFadeOut().inSeconds());

            if (std::abs (p.x - xIn) <= fadeHandleSize)
                return DragMode::fadeIn;

            if (std::abs (p.x - xOut) <= fadeHandleSize)
                return DragMode::fadeOut;
        }
    }

    if (p.x - x1 <= edgeZone)
        return DragMode::trimStart;

    if (x2 - p.x <= edgeZone)
        return DragMode::trimEnd;

    return DragMode::move;
}

//==============================================================================
void TrackLane::mouseMove (const juce::MouseEvent& e)
{
    if (auto* clip = findClipAt (e.x))
    {
        switch (getModeAt (*clip, e.getPosition()))
        {
            case DragMode::trimStart:
            case DragMode::trimEnd:
                setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
                break;

            case DragMode::fadeIn:
            case DragMode::fadeOut:
                setMouseCursor (juce::MouseCursor::PointingHandCursor);
                break;

            default:
                setMouseCursor (juce::MouseCursor::DraggingHandCursor);
                break;
        }
    }
    else
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }
}

void TrackLane::mouseDown (const juce::MouseEvent& e)
{
    auto& transport = session.getTransport();

    if (transport.isRecording() || getWidth() <= 0)
        return;

    grabKeyboardFocus();
    session.setFocusedTrack (trackIndex);

    auto* clip = findClipAt (e.x);

    if (e.mods.isPopupMenu())
    {
        if (clip != nullptr)
        {
            if (! session.isClipSelected (clip))
                session.selectClip (clip);

            showClipMenu (clip);
        }

        return;
    }

    drag = {};

    // Ctrl+click adds or removes a clip from the selection
    if (clip != nullptr && e.mods.isCommandDown())
    {
        session.toggleClipSelection (clip);
        return;
    }

    // Empty space: start a selection box and drop the playhead here (Ctrl keeps the current selection)
    if (clip == nullptr)
    {
        drag.mode = DragMode::box;
        drag.boxStartX = drag.boxEndX = e.x;

        if (! e.mods.isCommandDown())
            session.clearSelection();

        transport.setPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, xToTime (e.x))));
        return;
    }

    // Grabbing a clip that's already in a multi-selection keeps the selection
    if (! session.isClipSelected (clip))
        session.selectClip (clip);

    auto pos = clip->getPosition();

    drag.mode       = getModeAt (*clip, e.getPosition());
    drag.clip       = clip;
    drag.downTime   = xToTime (e.x);
    drag.origStart  = pos.getStart().inSeconds();
    drag.origEnd    = pos.getEnd().inSeconds();
    drag.origOffset = pos.getOffset().inSeconds();

    if (auto* acb = dynamic_cast<te::AudioClipBase*> (clip))
    {
        drag.origFadeIn  = acb->getFadeIn().inSeconds();
        drag.origFadeOut = acb->getFadeOut().inSeconds();
    }

    if (drag.mode == DragMode::move)
        for (auto& c : session.getSelectedClips())
            drag.moving.push_back ({ c, c->getPosition().getStart().inSeconds() });

    session.beginEdit();

    // Clicking a clip body drops the playhead there (handy for S to split)
    if (drag.mode == DragMode::move)
        transport.setPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, xToTime (e.x))));
}

void TrackLane::mouseDrag (const juce::MouseEvent& e)
{
    if (drag.mode == DragMode::none)
        return;

    if (drag.mode == DragMode::box)
    {
        drag.boxEndX = e.x;
        repaint();
        return;
    }

    if (drag.clip == nullptr)
        return;

    if (! drag.changed && e.getDistanceFromDragStart() < 3)
        return;

    applyDrag (e);
    drag.changed = true;
}

void TrackLane::mouseUp (const juce::MouseEvent&)
{
    if (drag.mode == DragMode::box)
    {
        if (std::abs (drag.boxEndX - drag.boxStartX) > 3)
            selectClipsInBox();

        drag = {};
        repaint();
        return;
    }

    if (drag.changed)
        session.save();

    drag = {};
}

void TrackLane::selectClipsInBox()
{
    const double t1 = xToTime (juce::jmin (drag.boxStartX, drag.boxEndX));
    const double t2 = xToTime (juce::jmax (drag.boxStartX, drag.boxEndX));

    juce::Array<te::Clip*> hits;

    for (auto& ct : clipThumbs)
    {
        auto pos = ct.clip->getPosition();

        if (pos.getEnd().inSeconds() > t1 && pos.getStart().inSeconds() < t2)
            hits.add (ct.clip.get());
    }

    session.addClipsToSelection (hits);
}

void TrackLane::applyDrag (const juce::MouseEvent& e)
{
    auto* clip = drag.clip.get();

    const bool snap = session.isSnapEnabled() && ! e.mods.isAltDown();
    auto snapT = [&] (double t) { return snap ? session.snapTime (t) : t; };

    const double delta = xToTime (e.x) - drag.downTime;
    constexpr double minLength = 0.05;

    switch (drag.mode)
    {
        case DragMode::move:
        {
            // Snap the clip you grabbed, then move the rest by the same amount
            double shift = juce::jmax (0.0, snapT (drag.origStart + delta)) - drag.origStart;

            for (auto& m : drag.moving)
                shift = juce::jmax (shift, -m.origStart);

            for (auto& m : drag.moving)
                if (m.clip->getParent() != nullptr)
                    m.clip->setStart (te::TimePosition::fromSeconds (m.origStart + shift), false, true);

            break;
        }

        case DragMode::trimStart:
        {
            // Can't pull the start back past the beginning of the audio
            const double earliest = juce::jmax (0.0, drag.origStart - drag.origOffset);
            const double newStart = juce::jlimit (earliest, drag.origEnd - minLength,
                                                  snapT (drag.origStart + delta));
            clip->setStart (te::TimePosition::fromSeconds (newStart), true, false);
            break;
        }

        case DragMode::trimEnd:
        {
            // Can't pull the end past the end of the audio
            const double maxLength = clip->getMaximumLength().inSeconds();
            const double latest = juce::jmax (drag.origStart + minLength,
                                              drag.origStart - drag.origOffset + maxLength);
            const double newEnd = juce::jlimit (drag.origStart + minLength, latest,
                                                snapT (drag.origEnd + delta));
            clip->setEnd (te::TimePosition::fromSeconds (newEnd), true);
            break;
        }

        case DragMode::fadeIn:
        {
            if (auto* acb = dynamic_cast<te::AudioClipBase*> (clip))
            {
                const double length = drag.origEnd - drag.origStart;
                const double newFade = juce::jlimit (0.0, juce::jmax (0.0, length - drag.origFadeOut),
                                                     drag.origFadeIn + delta);
                acb->setFadeIn (te::TimeDuration::fromSeconds (newFade));
            }
            break;
        }

        case DragMode::fadeOut:
        {
            if (auto* acb = dynamic_cast<te::AudioClipBase*> (clip))
            {
                const double length = drag.origEnd - drag.origStart;
                const double newFade = juce::jlimit (0.0, juce::jmax (0.0, length - drag.origFadeIn),
                                                     drag.origFadeOut - delta);
                acb->setFadeOut (te::TimeDuration::fromSeconds (newFade));
            }
            break;
        }

        default:
            break;
    }
}

//==============================================================================
void TrackLane::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Ctrl+scroll zooms around the mouse
    if (e.mods.isCommandDown())
    {
        if (wheel.deltaY != 0.0f)
            session.zoomAround (xToTime (e.x), wheel.deltaY > 0.0f ? 0.8 : 1.25);

        return;
    }

    // Shift+scroll (or a sideways trackpad swipe) scrolls the timeline
    if (e.mods.isShiftDown() || std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
    {
        const float d = wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY;
        session.scrollBy (-d * session.getViewLength() * 0.5);
        return;
    }

    // Plain scroll goes up to the viewport so tracks scroll vertically
    juce::Component::mouseWheelMove (e, wheel);
}

bool TrackLane::keyPressed (const juce::KeyPress& key)
{
    const bool hasSelection = ! session.getSelectedClips().empty();

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (hasSelection)
        {
            session.deleteSelectedClip();
            return true;
        }
    }

    // S (no modifiers) splits the selected clips at the playhead
    if (! key.getModifiers().isAnyModifierKeyDown()
        && (key.getKeyCode() == 'S' || key.getKeyCode() == 's'))
    {
        if (hasSelection)
        {
            session.splitSelectedClipAtPlayhead();
            return true;
        }
    }

    return false;
}

//==============================================================================
void TrackLane::showClipMenu (te::Clip* clip)
{
    juce::PopupMenu menu;
    auto* s = &session;

    if (auto* wac = dynamic_cast<te::WaveAudioClip*> (clip))
    {
        const int numTakes = wac->getNumTakes (false);
        te::WaveAudioClip::Ptr clipPtr (wac);

        if (numTakes > 1)
        {
            juce::PopupMenu takes;
            const int current = wac->getCurrentTake();

            for (int i = 0; i < numTakes; ++i)
                takes.addItem ("Take " + juce::String (i + 1), true, i == current,
                               [clipPtr, i] { clipPtr->setCurrentTake (i); });

            menu.addSubMenu ("Takes", takes);
            menu.addItem ("Comp Takes...", [s, clipPtr]
            {
                if (s->onShowComp)
                    s->onShowComp (*clipPtr);
            });
        }
        else
        {
            menu.addItem ("Comp Takes (loop-record to get takes)", false, false, nullptr);
        }

        menu.addSeparator();
    }

    if (auto* acb = dynamic_cast<te::AudioClipBase*> (clip))
    {
        te::AudioClipBase::Ptr acbPtr (acb);
        menu.addItem ("Stretch, Pitch & Gain...", [s, acbPtr]
        {
            if (s->onShowClipProps)
                s->onShowClipProps (*acbPtr);
        });

        menu.addItem ("Tune with KngTune", [s, idx = trackIndex]
        {
            if (! s->addKngTune (idx))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "KngTune not found",
                                                        "Put KngTune in your VST3 folder, then use File > Scan Plugins.");
        });

        menu.addSeparator();
    }

    if (auto* mc = dynamic_cast<te::MidiClip*> (clip))
    {
        te::MidiClip::Ptr midiPtr (mc);
        menu.addItem ("Open Piano Roll", [s, midiPtr]
        {
            if (s->onShowPianoRoll)
                s->onShowPianoRoll (*midiPtr);
        });

        menu.addSeparator();
    }

    menu.addItem ("Duplicate (Ctrl+D)", [s] { s->duplicateSelectedClip(); });
    menu.addItem ("Copy (Ctrl+C)",      [s] { s->copySelectedClip(); });
    menu.addItem ("Paste (Ctrl+V)",     [s] { s->pasteClip(); });
    menu.addSeparator();
    menu.addItem ("Split at Playhead (S)", [s] { s->splitSelectedClipAtPlayhead(); });
    menu.addItem ("Delete (Del)",          [s] { s->deleteSelectedClip(); });
    menu.showMenuAsync ({});
}

//==============================================================================
static bool isAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;mp3;aif;aiff;flac;ogg");
}

bool TrackLane::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& path : files)
        if (isAudioFile (juce::File (path)))
            return true;

    return false;
}

void TrackLane::filesDropped (const juce::StringArray& files, int x, int)
{
    double t = juce::jmax (0.0, session.snapTime (xToTime (x)));

    // Several files land back to back
    for (auto& path : files)
    {
        juce::File f (path);

        if (! isAudioFile (f))
            continue;

        const double end = session.importAudioFileAt (f, trackIndex, t);

        if (end > t)
            t = end;
    }
}

//==============================================================================
void TrackLane::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (session.getTransport().isRecording())
        return;

    // Double-click a MIDI clip to open its piano roll
    if (auto* clip = findClipAt (e.x))
    {
        if (auto* mc = dynamic_cast<te::MidiClip*> (clip))
            if (session.onShowPianoRoll)
                session.onShowPianoRoll (*mc);

        return;
    }

    // Double-click empty space on a MIDI track to make a new clip
    if (session.isMidiTrack (trackIndex))
        if (auto* mc = session.createMidiClip (trackIndex, xToTime (e.x)))
            if (session.onShowPianoRoll)
                session.onShowPianoRoll (*mc);
}
