#include "LatencyTester.h"

LatencyTester::LatencyTester()
{
    reset();
}

void LatencyTester::reset()
{
    counter = 0;
    floorLevel = 0.0f;
    detectedThisRun = false;

    for (auto& r : results)
        r.store (-1);

    finished.store (false);
}

void LatencyTester::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device != nullptr)
        sampleRate.store (device->getCurrentSampleRate());
}

void LatencyTester::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                                      float* const* outputChannelData, int numOutputChannels,
                                                      int numSamples,
                                                      const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

    if (finished.load())
        return;

    // Each run is 1 second: 0.3s to measure the noise floor, click at 0.4s, listen for the rest
    const double sr = sampleRate.load();
    const juce::int64 period   = (juce::int64) sr;
    const juce::int64 floorEnd = (juce::int64) (sr * 0.3);
    const juce::int64 clickAt  = (juce::int64) (sr * 0.4);
    constexpr int clickLength = 10;

    for (int i = 0; i < numSamples; ++i)
    {
        const int run = (int) (counter / period);

        if (run >= numRuns)
        {
            finished.store (true);
            return;
        }

        const juce::int64 pos = counter % period;

        if (pos == 0)
        {
            floorLevel = 0.0f;
            detectedThisRun = false;
        }

        const float in = (numInputChannels > 0 && inputChannelData[0] != nullptr)
                           ? std::abs (inputChannelData[0][i]) : 0.0f;

        if (pos < floorEnd)
            floorLevel = juce::jmax (floorLevel, in);

        // A short sharp click
        if (pos >= clickAt && pos < clickAt + clickLength)
        {
            const float v = (pos - clickAt) < clickLength / 2 ? 0.9f : -0.9f;

            for (int ch = 0; ch < numOutputChannels; ++ch)
                if (outputChannelData[ch] != nullptr)
                    outputChannelData[ch][i] = v;
        }

        // First input sample well above the noise floor is the click coming back
        if (! detectedThisRun && pos > clickAt)
        {
            const float threshold = juce::jmax (0.05f, floorLevel * 6.0f);

            if (in > threshold)
            {
                results[(size_t) run].store ((int) (pos - clickAt));
                detectedThisRun = true;
            }
        }

        ++counter;
    }
}
