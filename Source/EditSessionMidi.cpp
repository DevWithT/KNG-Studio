#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
void EditSession::addMidiTrack()
{
    addTrack();

    if (auto* t = getTrack (getNumTracks() - 1))
    {
        t->state.setProperty ("kngTrackType", "midi", nullptr);
        t->setName ("MIDI " + juce::String (getNumTracks()));
    }

    syncTrackState();
    save();
}

bool EditSession::isMidiTrack (int index)
{
    if (auto* t = getTrack (index))
        return t->state.getProperty ("kngTrackType").toString() == "midi";

    return false;
}

//==============================================================================
te::MidiClip* EditSession::createMidiClip (int trackIndex, double startSeconds)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr || getTransport().isRecording())
        return nullptr;

    // Start on the bar you clicked in, 4 bars long
    const double bar = 4.0 * 60.0 / juce::jmax (1.0, getBpm());
    const double start = juce::jmax (0.0, std::floor (startSeconds / bar) * bar);

    beginEdit();

    auto clip = track->insertMIDIClip (te::TimeRange (te::TimePosition::fromSeconds (start),
                                                      te::TimePosition::fromSeconds (start + bar * 4.0)),
                                       nullptr);

    if (clip == nullptr)
        return nullptr;

    selectClip (clip.get());
    save();
    return clip.get();
}

//==============================================================================
void EditSession::setInstrument (int trackIndex, const juce::PluginDescription& desc)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return;

    auto plugin = edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, desc);

    if (plugin == nullptr)
        return;

    beginEdit();

    edit->getUndoManager().setCurrentTransactionName ("Change instrument");

    // One instrument per track, so swap out the old one
    for (auto p : track->pluginList.getPlugins())
        if (p->isSynth())
            p->deleteFromParent();

    track->pluginList.insertPlugin (plugin, 0, nullptr);
    plugin->showWindowExplicitly();
    save();
}

te::Plugin* EditSession::getInstrument (int trackIndex)
{
    if (auto* t = getTrack (trackIndex))
        for (auto* p : t->pluginList)
            if (p->isSynth())
                return p;

    return nullptr;
}

void EditSession::removeInstrument (int trackIndex)
{
    if (auto* inst = getInstrument (trackIndex))
    {
        beginEdit();
        edit->getUndoManager().setCurrentTransactionName ("Remove instrument");
        inst->deleteFromParent();
        save();
    }
}

//==============================================================================
// Live MIDI: the selected instrument track plays from any connected MIDI keyboard or pad.
// Arming a track to record works as before and keeps its keyboard.

void EditSession::routeLiveMidi()
{
    auto* track = dynamic_cast<te::AudioTrack*> (selectedTrackPtr.get());

    if (track == nullptr || getTransport().isRecording())
        return;

    // Only tracks that make sound from notes
    bool hasInstrument = false;

    for (auto* p : track->pluginList)
    {
        if (p->isSynth())
        {
            hasInstrument = true;
            break;
        }
    }

    if (! hasInstrument)
        return;

    for (auto instance : edit->getAllInputDevices())
    {
        if (instance->getInputDevice().getDeviceType() != te::InputDevice::physicalMidiDevice)
            continue;

        // Already feeding this track
        const auto targets = instance->getTargets();

        if (targets.size() == 1 && targets[0] == track->itemID)
            continue;

        // A track armed to record keeps its keyboard
        bool armed = false;

        for (auto id : targets)
            armed = armed || instance->isRecordingEnabled (id);

        if (armed)
            continue;

        [[maybe_unused]] auto result = instance->setTarget (track->itemID, true, nullptr, 0);
    }
}

//==============================================================================
// Hotplug: Tracktion rescans MIDI devices every few seconds and broadcasts when the list changes.
// MainComponent calls this on that broadcast so a keyboard or pad plugged in later just works.

void EditSession::refreshMidiInputs()
{
    auto& dm = engine.getDeviceManager();
    juce::StringArray present;

    for (int i = 0; i < dm.getNumMidiInDevices(); ++i)
    {
        auto mid = dm.getMidiInDevice (i);

        if (mid == nullptr)
            continue;

        const auto id = mid->getDeviceID();
        present.add (id);

        // Only a device we have not seen yet gets switched on, so one turned off in Settings stays off
        if (! knownMidiInputs.contains (id))
            mid->setEnabled (true);

        if (mid->getMonitorMode() != te::InputDevice::MonitorMode::on)
            mid->setMonitorMode (te::InputDevice::MonitorMode::on);
    }

    knownMidiInputs = present;
    routeLiveMidi();
}

//==============================================================================
// Computer keyboard piano: a note on or off sent straight to the selected instrument track.

void EditSession::playLiveNote (int note, bool down)
{
    auto* track = dynamic_cast<te::AudioTrack*> (selectedTrackPtr.get());

    if (track == nullptr)
        return;

    // Only tracks that make sound from notes
    bool hasInstrument = false;

    for (auto* p : track->pluginList)
    {
        if (p->isSynth())
        {
            hasInstrument = true;
            break;
        }
    }

    if (! hasInstrument)
        return;

    const auto message = down ? juce::MidiMessage::noteOn (1, note, (juce::uint8) 100)
                              : juce::MidiMessage::noteOff (1, note);

    track->injectLiveMidiMessage (message, {});
}
