#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <array>
#include <atomic>

/** Plays a click out of the speakers and times how long it takes to come back in. */
class LatencyTester : public juce::AudioIODeviceCallback
{
public:
    static constexpr int numRuns = 5;

    LatencyTester();

    void reset();
    bool isFinished() const                 { return finished.load(); }
    double getSampleRate() const            { return sampleRate.load(); }
    int getResult (int run) const           { return results[(size_t) run].load(); }

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override {}

private:
    std::atomic<double> sampleRate { 48000.0 };
    std::atomic<bool> finished { false };
    std::array<std::atomic<int>, numRuns> results;

    // Audio thread only
    juce::int64 counter = 0;
    float floorLevel = 0.0f;
    bool detectedThisRun = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LatencyTester)
};
