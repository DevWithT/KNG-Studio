#include "EditSession.h"

static juce::File getLatencyFile()
{
    auto root = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KNG Studio");
    root.createDirectory();
    return root.getChildFile ("latency.txt");
}

double EditSession::getLatencyOffsetMs()
{
    auto f = getLatencyFile();
    return f.existsAsFile() ? f.loadFileAsString().trim().getDoubleValue() : 0.0;
}

void EditSession::setLatencyOffsetMs (double ms)
{
    getLatencyFile().replaceWithText (juce::String (ms, 2));

    auto& dm = engine.getDeviceManager();

    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
        if (auto wip = dm.getWaveInDevice (i))
            wip->setRecordAdjustmentMs (ms);

    edit->restartPlayback();
}
