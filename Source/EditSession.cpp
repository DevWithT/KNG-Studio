#include "EditSession.h"

namespace te = tracktion;

static juce::File getSessionFile()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("KNG Studio")
                      .getChildFile ("Untitled");
    folder.createDirectory();
    return folder.getChildFile ("Untitled.tracktionedit");
}

EditSession::EditSession (te::Engine& engineToUse)
    : engine (engineToUse)
{
    auto file = getSessionFile();

    if (file.existsAsFile())
        edit = te::loadEditFromFile (engine, file);
    else
        edit = te::createEmptyEdit (engine, file);

    edit->playInStopEnabled = true;
    edit->ensureNumberOfAudioTracks (1);

    refreshInputs();
    save();
}

EditSession::~EditSession()
{
    getTransport().stop (false, false);
    save();
}

te::AudioTrack* EditSession::getTrack()
{
    edit->ensureNumberOfAudioTracks (1);
    return te::getAudioTracks (*edit)[0];
}

te::InputDeviceInstance* EditSession::findFirstWaveInput()
{
    for (auto instance : edit->getAllInputDevices())
        if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
            return instance;

    return nullptr;
}

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
            wip->setMonitorMode (te::InputDevice::MonitorMode::automatic);
            wip->setEnabled (true);
        }
    }

    edit->getTransport().ensureContextAllocated();

    auto* track = getTrack();
    auto* input = findFirstWaveInput();

    if (track != nullptr && input != nullptr)
    {
        [[maybe_unused]] auto result = input->setTarget (track->itemID, true, &edit->getUndoManager(), 0);
        input->setRecordingEnabled (track->itemID, armed);
    }

    edit->restartPlayback();
}

void EditSession::setArmed (bool shouldBeArmed)
{
    armed = shouldBeArmed;

    auto* track = getTrack();
    auto* input = findFirstWaveInput();

    if (track != nullptr && input != nullptr)
        input->setRecordingEnabled (track->itemID, armed);
}

void EditSession::play()
{
    getTransport().play (false);
}

void EditSession::record()
{
    if (! armed)
        setArmed (true);

    getTransport().record (false);
}

void EditSession::stop()
{
    auto& transport = getTransport();
    const bool wasRecording = transport.isRecording();

    transport.stop (false, false);
    transport.setPosition (te::TimePosition());

    if (wasRecording)
        save();
}

void EditSession::save()
{
    te::EditFileOperations (*edit).save (true, true, false);
}
