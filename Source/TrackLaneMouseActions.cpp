#include "TrackLane.h"

namespace te = tracktion;

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

        juce::PopupMenu chop;

        for (int n : { 4, 8, 16 })
            chop.addItem (juce::String (n) + " slices", [s, acbPtr, n] { s->chopClipToPads (*acbPtr, n, false); });

        chop.addItem ("At markers", [s, acbPtr] { s->chopClipToPads (*acbPtr, 16, true); });
        menu.addSubMenu ("Chop to Pads", chop);

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
        // Split one copy of a linked pattern into its own new pattern
        if (mc->state.hasProperty ("kngPattern"))
            menu.addItem ("Make Unique", [s, midiPtr] { s->makePatternUnique (*midiPtr); });

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
