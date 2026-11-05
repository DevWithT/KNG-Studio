#include "EditSession.h"

namespace te = tracktion;

EditSession::EditSession (te::Engine& engineToUse, const juce::File& editFileToUse)
    : engine (engineToUse), editFile (editFileToUse)
{
    editFile.getParentDirectory().createDirectory();

    if (editFile.existsAsFile())
        edit = te::loadEditFromFile (engine, editFile);
    else
        edit = te::createEmptyEdit (engine, editFile);

    edit->playInStopEnabled = true;
    edit->alwaysUseRelativePaths = true;

    // Restore this project's zoom
    viewStart  = juce::jmax (0.0, (double) edit->state.getProperty ("kngViewStart", 0.0));
    viewLength = juce::jlimit (1.0, 600.0, (double) edit->state.getProperty ("kngViewLength", 60.0));
    edit->ensureNumberOfAudioTracks (1);

    syncTrackState();
    refreshInputs();
    save();
}

EditSession::~EditSession()
{
    getTransport().stop (false, false);
    save();
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

    selectedClip = nullptr;
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
juce::Array<te::InputDeviceInstance*> EditSession::getWaveInputs()
{
    juce::Array<te::InputDeviceInstance*> result;

    for (auto instance : edit->getAllInputDevices())
        if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
            result.add (instance);

    return result;
}

juce::StringArray EditSession::getInputNames()
{
    juce::StringArray names;

    for (auto* input : getWaveInputs())
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
        // One input can only feed one armed track, so disarm any other track using it
        for (int j = 0; j < (int) trackArmed.size(); ++j)
            if (j != trackIndex
                && trackArmed[(size_t) j]
                && trackInputs[(size_t) j] == trackInputs[(size_t) trackIndex])
                applyArm (j, false);
    }

    applyArm (trackIndex, shouldBeArmed);
}

void EditSession::applyArm (int trackIndex, bool shouldBeArmed)
{
    auto* track = getTrack (trackIndex);
    auto inputs = getWaveInputs();
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
        }
    }

    edit->getTransport().ensureContextAllocated();
    syncTrackState();

    const int numInputs = getWaveInputs().size();

    for (auto& in : trackInputs)
        if (in >= numInputs)
            in = 0;

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
void EditSession::selectClip (te::Clip* clip)
{
    selectedClip = clip;
}

void EditSession::deleteSelectedClip()
{
    if (selectedClip == nullptr || getTransport().isRecording())
        return;

    auto clip = selectedClip;
    selectedClip = nullptr;

    clip->removeFromParent();
    save();
}
