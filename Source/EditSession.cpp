#include "EditSession.h"

namespace te = tracktion;

EditSession::EditSession (te::Engine& engineToUse, const juce::File& editFileToUse)
    : engine (engineToUse), editFile (editFileToUse)
{
    editFile.getParentDirectory().createDirectory();

    // A leftover lock file means the last session didn't close cleanly
    recoveredFromCrash = getLockFile().existsAsFile();

    if (editFile.existsAsFile())
        edit = te::loadEditFromFile (engine, editFile);
    else
        edit = te::createEmptyEdit (engine, editFile);

    edit->playInStopEnabled = true;
    edit->alwaysUseRelativePaths = true;

    // Restore this project's zoom (a new project starts with 8 bars across)
    const double barSeconds = 4.0 * 60.0 / juce::jmax (1.0, edit->tempoSequence.getBpmAt (te::TimePosition()));
    viewStart  = juce::jmax (0.0, (double) edit->state.getProperty ("kngViewStart", 0.0));
    viewLength = juce::jlimit (1.0, 600.0, (double) edit->state.getProperty ("kngViewLength", barSeconds * 8.0));

    edit->ensureNumberOfAudioTracks (1);

    syncTrackState();
    refreshInputs();

    // Mark this session as open, and start the autosave clock
    getLockFile().replaceWithText (juce::Time::getCurrentTime().toString (true, true));
    lastBackupTime = juce::Time::getCurrentTime();
    startTimer (60 * 1000);

    save();
}

EditSession::~EditSession()
{
    stopTimer();
    getTransport().stop (false, false);
    save();

    // Clean exit, so no crash warning next time
    getLockFile().deleteFile();
}

//==============================================================================
void EditSession::syncTrackState()
{
    const auto n = (size_t) getNumTracks();
    trackInputs.resize (n, 0);
    trackArmed.resize (n, false);
}

int EditSession::getNumTracks()
{
    return te::getAudioTracks (*edit).size();
}

te::AudioTrack* EditSession::getTrack (int index)
{
    return te::getAudioTracks (*edit)[index];
}

void EditSession::addTrack()
{
    edit->ensureNumberOfAudioTracks (getNumTracks() + 1);
    syncTrackState();

    // Take the lowest free lane number, so deleting Tracks 1 to 5 and adding one gives Track 1 again
    juce::Array<int> used;

    for (int i = 0; i < getNumTracks() - 1; ++i)
    {
        if (auto* t = getTrack (i))
        {
            const auto name = t->getName();

            if (name.startsWith ("Track ") && name.substring (6).trim().containsOnly ("0123456789"))
                used.add (name.substring (6).trim().getIntValue());
        }
    }

    int number = 1;

    while (used.contains (number))
        ++number;

    if (auto* t = getTrack (getNumTracks() - 1))
        t->setName ("Track " + juce::String (number));
}

void EditSession::deleteTrack (int index)
{
    if (getTransport().isRecording())
        return;

    auto* track = getTrack (index);

    if (track == nullptr)
        return;

    syncTrackState();

    if (isArmed (index))
        applyArm (index, false);

    clearSelection();
    beginEdit();

    // A lane's pattern blocks live on the Rack tracks, tagged with the lane. They go with it, so a new lane starts empty.
    if (isLaneTrack (index))
    {
        const auto laneId = getLaneId (index);
        const double bank = getBankStart() - 0.001;
        juce::Array<te::Clip*> blocks;

        if (laneId.isNotEmpty())
            for (int i = 0; i < getNumTracks(); ++i)
                if (i != index)
                    if (auto* t = getTrack (i))
                        for (auto* c : t->getClips())
                            if (c->state.getProperty ("kngLane").toString() == laneId && c->getPosition().getStart().inSeconds() < bank)
                                blocks.add (c);

        for (auto* c : blocks)
            c->removeFromParent();
    }

    edit->getUndoManager().setCurrentTransactionName ("Delete lane");
    edit->deleteTrack (track);

    if (juce::isPositiveAndBelow (index, (int) trackInputs.size()))
    {
        trackInputs.erase (trackInputs.begin() + index);
        trackArmed.erase (trackArmed.begin() + index);
    }

    syncTrackState();
    save();
}

//==============================================================================
juce::Array<te::InputDeviceInstance*> EditSession::getInputsFor (int trackIndex)
{
    const bool wantMidi = isMidiTrack (trackIndex);
    juce::Array<te::InputDeviceInstance*> result;

    for (auto instance : edit->getAllInputDevices())
    {
        const auto type = instance->getInputDevice().getDeviceType();

        if (wantMidi ? type == te::InputDevice::physicalMidiDevice
                     : type == te::InputDevice::waveDevice)
            result.add (instance);
    }

    return result;
}

juce::StringArray EditSession::getInputNames (int trackIndex)
{
    juce::StringArray names;

    for (auto* input : getInputsFor (trackIndex))
        names.add (input->getInputDevice().getName());

    return names;
}

int EditSession::getTrackInput (int trackIndex) const
{
    return juce::isPositiveAndBelow (trackIndex, (int) trackInputs.size())
             ? trackInputs[(size_t) trackIndex] : 0;
}

void EditSession::setTrackInput (int trackIndex, int inputIndex)
{
    syncTrackState();

    if (! juce::isPositiveAndBelow (trackIndex, (int) trackInputs.size()))
        return;

    const bool wasArmed = trackArmed[(size_t) trackIndex];

    if (wasArmed)
        applyArm (trackIndex, false);

    trackInputs[(size_t) trackIndex] = juce::jmax (0, inputIndex);

    if (wasArmed)
        setArmed (trackIndex, true);
}

//==============================================================================
bool EditSession::isArmed (int trackIndex) const
{
    return juce::isPositiveAndBelow (trackIndex, (int) trackArmed.size())
             && trackArmed[(size_t) trackIndex];
}

bool EditSession::isAnyTrackArmed() const
{
    for (bool armed : trackArmed)
        if (armed)
            return true;

    return false;
}

void EditSession::setArmed (int trackIndex, bool shouldBeArmed)
{
    syncTrackState();

    if (! juce::isPositiveAndBelow (trackIndex, (int) trackArmed.size()))
        return;

    if (shouldBeArmed)
    {
        // One input can only feed one armed track, so disarm any other track of the same kind using it
        const bool midi = isMidiTrack (trackIndex);

        for (int j = 0; j < (int) trackArmed.size(); ++j)
            if (j != trackIndex
                && trackArmed[(size_t) j]
                && isMidiTrack (j) == midi
                && trackInputs[(size_t) j] == trackInputs[(size_t) trackIndex])
                applyArm (j, false);
    }

    applyArm (trackIndex, shouldBeArmed);

    // Arming turns Mon on, disarming turns it off (this also clears any lane disarmed above)
    for (int j = 0; j < (int) trackArmed.size(); ++j)
    {
        if (auto* t = getTrack (j))
        {
            if (j == trackIndex && trackArmed[(size_t) j])
                t->state.setProperty ("kngMonitor", true, nullptr);
            else if (! trackArmed[(size_t) j])
                t->state.setProperty ("kngMonitor", false, nullptr);
        }
    }

    updateMonitoring();
}

void EditSession::applyArm (int trackIndex, bool shouldBeArmed)
{
    auto* track = getTrack (trackIndex);
    auto inputs = getInputsFor (trackIndex);
    auto* input = inputs[trackInputs[(size_t) trackIndex]];

    trackArmed[(size_t) trackIndex] = shouldBeArmed && track != nullptr && input != nullptr;

    if (track == nullptr || input == nullptr)
        return;

    if (shouldBeArmed)
    {
        [[maybe_unused]] auto result = input->setTarget (track->itemID, true, nullptr, 0);
        input->setRecordingEnabled (track->itemID, true);
        edit->restartPlayback();
    }
    else
    {
        input->setRecordingEnabled (track->itemID, false);
    }
}

//==============================================================================
void EditSession::refreshInputs()
{
    auto& dm = engine.getDeviceManager();

    // Changing the channel config rebuilds the device list, so it gets its own loop
    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
        if (auto wip = dm.getWaveInDevice (i))
            wip->setChannelConfiguration (te::ChannelConfiguration::mono());

    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
    {
        if (auto wip = dm.getWaveInDevice (i))
        {
            wip->setMonitorMode (monitoringEnabled ? te::InputDevice::MonitorMode::automatic
                                                   : te::InputDevice::MonitorMode::off);
            wip->setEnabled (true);
            wip->setRecordAdjustmentMs (getLatencyOffsetMs());
        }
    }

    // MIDI keyboards are always heard through the selected instrument, armed or not
    knownMidiInputs.clear();
    for (int i = 0; i < dm.getNumMidiInDevices(); ++i)
    {
        if (auto mid = dm.getMidiInDevice (i))
        {
            mid->setEnabled (true);
            mid->setMonitorMode (te::InputDevice::MonitorMode::on);
            knownMidiInputs.addIfNotAlreadyThere (mid->getDeviceID());
        }
    }

    edit->getTransport().ensureContextAllocated();
    syncTrackState();

    for (int i = 0; i < (int) trackInputs.size(); ++i)
        if (trackInputs[(size_t) i] >= getInputsFor (i).size())
            trackInputs[(size_t) i] = 0;

    for (int i = 0; i < (int) trackArmed.size(); ++i)
        if (trackArmed[(size_t) i])
            applyArm (i, true);

    edit->restartPlayback();
}

void EditSession::setMonitoring (bool shouldMonitor)
{
    monitoringEnabled = shouldMonitor;

    auto& dm = engine.getDeviceManager();

    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
        if (auto wip = dm.getWaveInDevice (i))
            wip->setMonitorMode (monitoringEnabled ? te::InputDevice::MonitorMode::automatic
                                                   : te::InputDevice::MonitorMode::off);

    edit->restartPlayback();
}

//==============================================================================
bool EditSession::isTrackMonitor (int trackIndex)
{
    if (auto* t = getTrack (trackIndex))
        return (bool) t->state.getProperty ("kngMonitor", false);

    return false;
}

void EditSession::setTrackMonitor (int trackIndex, bool shouldMonitor)
{
    if (auto* t = getTrack (trackIndex))
        t->state.setProperty ("kngMonitor", shouldMonitor, nullptr);

    updateMonitoring();
}

// The engine monitors per input device, not per lane: you hear your input while any armed lane has Mon on
void EditSession::updateMonitoring()
{
    bool any = false;

    for (int i = 0; i < getNumTracks(); ++i)
        if (isArmed (i) && isTrackMonitor (i))
            any = true;

    if (any != monitoringEnabled)
        setMonitoring (any);
}

//==============================================================================

void EditSession::selectClip (te::Clip* clip)
{
    selection.clear();
    selectedClip = clip;

    if (clip != nullptr)
        selection.push_back (clip);
}

void EditSession::deleteSelectedClip()
{
    auto clips = getSelectedClips();

    if (clips.empty() || getTransport().isRecording())
        return;

    clearSelection();
    beginEdit();

    edit->getUndoManager().setCurrentTransactionName ("Delete clip");

    for (auto& c : clips)
        c->removeFromParent();

    save();
}
