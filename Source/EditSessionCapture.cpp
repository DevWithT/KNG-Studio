#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
// Capture: Tracktion keeps the last 30 seconds from every MIDI keyboard. This asks it for a clip.

juce::String EditSession::captureMidi()
{
    if (getTransport().isRecording())
        return "Stop recording first. Capture is for what you played without recording.";

    juce::Array<te::InputDeviceInstance*> midiInputs;

    for (auto* in : edit->getAllInputDevices())
        if (in != nullptr && in->getInputDevice().isMidi())
            midiInputs.add (in);

    if (midiInputs.isEmpty())
        return "No MIDI keyboard or pad is connected.";

    const bool wasPlaying = getTransport().isPlaying();
    const auto playhead = getTransport().getPosition();

    juce::Array<te::Clip*> made;

    te::InputDevice::setRetrospectiveLock (engine, midiInputs, true);

    for (auto* in : midiInputs)
        made.addArray (in->applyRetrospectiveRecord (false));

    te::InputDevice::setRetrospectiveLock (engine, midiInputs, false);

    if (made.isEmpty())
        return "Nothing to capture yet. Select a track with an instrument, then play something on your MIDI keyboard and try again.";

    // While playing, the engine puts the phrase where you played it on the timeline.
    // While stopped, it lands at the playhead.
    if (! wasPlaying)
        for (auto* c : made)
            c->setStart (playhead, false, true);

    clearSelection();
    addClipsToSelection (made);
    save();
    return {};
}
