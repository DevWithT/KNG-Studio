#include "MainComponent.h"
#include "KngSamplerPlugin.h"
#include "KngSynthPlugin.h"
#include "KngEqPlugin.h"
#include "KngCompPlugin.h"
#include "KngVerbPlugin.h"
#include "KngDelayPlugin.h"
#include "KngSatPlugin.h"
#include "KngDeEssPlugin.h"
#include "KngLimiterPlugin.h"
#include "KngDoublerPlugin.h"
#include "KngLoudnessPlugin.h"
#include "KngFattenerPlugin.h"
#include "Kng808Plugin.h"
#include "KngAirPlugin.h"
#include "KngGatePlugin.h"
#include "KngWidthPlugin.h"
#include "KngRadioPlugin.h"
#include "KngThrowPlugin.h"
#include "PluginWindow.h"
#include "MidiMapDialog.h"
#include "KngPopOut.h"

//==============================================================================
void MainComponent::togglePlayStop()
{
    if (session == nullptr)
        return;

    if (session->getTransport().isPlaying())
        session->stop();
    else
        session->play();
}

void MainComponent::zoomToFit()
{
    if (session == nullptr)
        return;

    const double length = juce::jmax (10.0, session->getSongEnd());
    session->setView (0.0, length * 1.05);
}

void MainComponent::showShortcuts()
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Keyboard Shortcuts",
        "Space\tPlay / Stop\n"
        "Home\tGo to start\n"
        "M\tMixer\n"
        "R\tRack\n"
        "I\tTrack inspector\n"
        "P\tPatterns list\n"
        "S\tSplit selected clip at playhead\n"
        "Del\tDelete selected clip\n"
        "Ctrl+Z / Ctrl+Y\tUndo / Redo\n"
        "Ctrl+C / Ctrl+V\tCopy / Paste clip\n"
        "Ctrl+D\tDuplicate clip\n"
        "Ctrl+X\tCut clip\n"
        "Ctrl+A / Ctrl+Shift+A\tSelect all / select none\n"
        "Ctrl+I\tInvert selection\n"
        "Ctrl+R\tRe-take (while recording)\n"
        "Ctrl+L\tMIDI Learn on / off\n"
        "Ctrl+K\tComputer keyboard piano on / off\n"
        "Ctrl+1 / 2 / 3\tLoad window layout 1 / 2 / 3\n"
        "Ctrl+Shift+1 / 2 / 3\tSave the window layout\n"
        "Up / Down\tOctave (while the piano is on)\n"
        "Ctrl+N / O / S\tNew / Open / Save\n"
        "Scroll\tZoom in / out\n"
        "Shift / Alt+Scroll\tScroll sideways / lanes up and down\n"
        "Ctrl+= / Ctrl+-\tZoom in / out\n"
        "Alt+Drag\tMove without snapping\n\n"
        "Piano roll:\n"
        "Q\tQuantize\n"
        "G\tSlide into the selected note (808 glides)\n"
        "Ctrl+A\tSelect all notes");
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    // Keys don't reach the project while the start screen is up
    if (startScreenVisible)
        return false;

    if (session == nullptr)
        return false;

    const auto mods = key.getModifiers();
    const int code  = key.getKeyCode();

    // Piano on: letter and number keys play notes instead of triggering shortcuts
    if (pianoKeysOn && ! mods.isAnyModifierKeyDown() && handlePianoKey (key))
        return true;

    // Left / Right move the playhead a beat (Ctrl: a bar). Hold to skim.
    if (session != nullptr && (key.isKeyCode (juce::KeyPress::leftKey) || key.isKeyCode (juce::KeyPress::rightKey)))
    {
        auto& tempo = session->getEdit().tempoSequence;
        auto& transport = session->getTransport();

        const double step = key.getModifiers().isCommandDown() ? 4.0 : 1.0;
        const double beat = tempo.toBeats (transport.getPosition()).inBeats();
        const bool forward = key.isKeyCode (juce::KeyPress::rightKey);

        const double target = forward ? (std::floor (beat / step + 1e-6) + 1.0) * step
                                      : (std::ceil (beat / step - 1e-6) - 1.0) * step;

        transport.setPosition (tempo.toTime (tracktion::BeatPosition::fromBeats (juce::jmax (0.0, target))));
        return true;
    }

    if (mods.isCommandDown())
    {
        // Ctrl+Z undo, Ctrl+Shift+Z redo
        if (code == 'Z' || code == 'z')
        {
            if (mods.isShiftDown())
                session->redo();
            else
                session->undo();

            return true;
        }

        if (code == 'Y' || code == 'y')  { session->redo();                  return true; }
        if (code == 'S' || code == 's')  { saveProject();                    return true; }
        if (code == 'O' || code == 'o')  { openProject();                    return true; }
        if (code == 'N' || code == 'n')  { newProject();                     return true; }
        if (code == 'D' || code == 'd')  { session->duplicateSelectedClip(); return true; }
        if (code == 'C' || code == 'c')  { session->copySelectedClip();      return true; }
        if (code == 'V' || code == 'v')  { session->pasteClip();             return true; }
        if (code == 'A' || code == 'a')
        {
            if (mods.isShiftDown())
                session->clearSelection();
            else
                session->selectAllClips();

            return true;
        }

        if (code == 'X' || code == 'x')  { session->cutSelectedClip();       return true; }
        if (code == 'I' || code == 'i')  { session->invertSelection();       return true; }
        if (code == 'R' || code == 'r')  { session->retake();                return true; }
        if (code == 'L' || code == 'l')  { session->setMidiLearn (! session->isMidiLearn()); return true; }
        if (code == ',')                 { showSettings();                   return true; }
        if (code == 'K' || code == 'k')  { setPianoKeys (! pianoKeysOn);     return true; }

        // Ctrl+1, 2, 3 load window layout 1, 2, 3. Ctrl+Shift+1, 2, 3 save the current layout there.
        {
            int slot = 0;

            if (code == '1' || code == '!')      slot = 1;
            else if (code == '2' || code == '@') slot = 2;
            else if (code == '3' || code == '#') slot = 3;

            if (slot > 0)
            {
                if (mods.isShiftDown())
                    saveLayout (slot);
                else
                    loadLayout (slot);

                return true;
            }
        }

        if (code == '=' || code == '+')
        {
            session->zoomAround (session->getViewStart() + session->getViewLength() * 0.5, 0.8);
            return true;
        }

        if (code == '-')
        {
            session->zoomAround (session->getViewStart() + session->getViewLength() * 0.5, 1.25);
            return true;
        }

        return false;
    }

    // D shows and hides the dock
    if (code == 'D' || code == 'd')
    {
        dockVisible = ! dockVisible;
        resized();
        return true;
    }

    // B shows and hides the browser
    if (code == 'B' || code == 'b')
    {
        browserVisible = ! browserVisible;
        resized();
        return true;
    }

    // M opens and closes the mixer
    if (code == 'M' || code == 'm')
    {
        toggleMixer();
        return true;
    }

    // R opens and closes the Rack
    if (code == 'R' || code == 'r')
    {
        toggleRack();
        return true;
    }

    // I shows and hides the track inspector
    if (code == 'I' || code == 'i')
    {
        inspectorVisible = ! inspectorVisible;
        resized();
        return true;
    }

    // P shows and hides the Patterns list
    if (code == 'P' || code == 'p')
    {
        patternsVisible = ! patternsVisible;
        resized();
        return true;
    }

    // [ and ] jump between markers
    if (code == '[')
    {
        session->jumpToMarker (-1);
        return true;
    }

    if (code == ']')
    {
        session->jumpToMarker (1);
        return true;
    }

    if (code == juce::KeyPress::homeKey)
    {
        session->getTransport().setPosition (tracktion::TimePosition());
        return true;
    }

    // Space toggles play/stop
    if (code == juce::KeyPress::spaceKey)
    {
        togglePlayStop();
        return true;
    }

    return false;
}

//==============================================================================
// Computer keyboard piano

namespace
{
    // Two rows laid out like a piano. The Z row is the lower octave, the Q row the upper one.
    const juce::String pianoLowerRow = "ZSXDCVGBHNJM,";
    const juce::String pianoUpperRow = "Q2W3ER5T6Y7UI";
}

int MainComponent::pianoNoteFor (const juce::KeyPress& key) const
{
    const auto c = juce::CharacterFunctions::toUpperCase (key.getTextCharacter());

    int i = pianoLowerRow.indexOfChar (c);

    if (i >= 0)
        return pianoBase + i;

    i = pianoUpperRow.indexOfChar (c);

    if (i >= 0)
        return pianoBase + 12 + i;

    return -1;
}

void MainComponent::setPianoKeys (bool on)
{
    releasePianoKeys();
    pianoKeysOn = on;
    repaint (0, getHeight() - statusHeight, getWidth(), statusHeight);
}

void MainComponent::releasePianoKeys()
{
    if (session != nullptr)
        for (auto& held : heldPianoKeys)
            session->playLiveNote (held.second, false);

    heldPianoKeys.clear();
}

bool MainComponent::handlePianoKey (const juce::KeyPress& key)
{
    if (key.isKeyCode (juce::KeyPress::upKey) || key.isKeyCode (juce::KeyPress::downKey))
    {
        releasePianoKeys();
        pianoBase = juce::jlimit (0, 96, pianoBase + (key.isKeyCode (juce::KeyPress::upKey) ? 12 : -12));
        repaint (0, getHeight() - statusHeight, getWidth(), statusHeight);
        return true;
    }

    const int note = pianoNoteFor (key);

    if (note < 0 || note > 127)
        return false;

    // Holding a key repeats keyPressed, so only the first press plays the note
    if (heldPianoKeys.find (key.getKeyCode()) == heldPianoKeys.end())
    {
        heldPianoKeys[key.getKeyCode()] = note;
        session->playLiveNote (note, true);
    }

    return true;
}

bool MainComponent::keyStateChanged (bool)
{
    if (heldPianoKeys.empty())
        return false;

    bool released = false;

    for (auto it = heldPianoKeys.begin(); it != heldPianoKeys.end();)
    {
        if (! juce::KeyPress::isKeyCurrentlyDown (it->first))
        {
            if (session != nullptr)
                session->playLiveNote (it->second, false);

            it = heldPianoKeys.erase (it);
            released = true;
        }
        else
        {
            ++it;
        }
    }

    return released;
}
