#include "KngLoudnessPlugin.h"

namespace te = tracktion;

const char* KngLoudnessPlugin::xmlTypeName = "kngloudness";

void KngLoudnessPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;

    // K-weighting: a gentle high shelf, then a low cut
    shelfCoefs    = KngEq::makeBiquad (KngEq::highShelf, 1681.974, 3.99984, 0.7071752, sampleRate);
    highPassCoefs = KngEq::makeBiquad (KngEq::lowCut, 38.13547, 0.0, 0.5003270, sampleRate);

    blockLength = juce::jmax (1, juce::roundToInt (0.1 * sampleRate));
    reset();
}

void KngLoudnessPlugin::reset()
{
    for (auto& ch : states)
        for (auto& s : ch)
            s.reset();

    acc = 0.0;
    count = 0;
}

void KngLoudnessPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    float blockPeak = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        for (int ch = 0; ch < chans; ++ch)
        {
            const float x = buffer.getSample (ch, fc.bufferStartSample + i);
            const float y = states[(size_t) ch][1].process (states[(size_t) ch][0].process (x, shelfCoefs), highPassCoefs);
            acc += (double) y * y;
            blockPeak = juce::jmax (blockPeak, std::abs (x));
        }

        if (++count >= blockLength)
        {
            int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
            fifo.prepareToWrite (1, s1, n1, s2, n2);

            if (n1 > 0)
                fifoData[(size_t) s1] = (float) (acc / blockLength);
            else if (n2 > 0)
                fifoData[(size_t) s2] = (float) (acc / blockLength);

            fifo.finishedWrite (n1 + n2);
            acc = 0.0;
            count = 0;
        }
    }

    float current = peak.load();

    while (blockPeak > current && ! peak.compare_exchange_weak (current, blockPeak)) {}
}

int KngLoudnessPlugin::pullBlocks (float* dest, int maxBlocks)
{
    int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
    fifo.prepareToRead (maxBlocks, s1, n1, s2, n2);

    for (int i = 0; i < n1; ++i)
        dest[i] = fifoData[(size_t) (s1 + i)];

    for (int i = 0; i < n2; ++i)
        dest[n1 + i] = fifoData[(size_t) (s2 + i)];

    fifo.finishedRead (n1 + n2);
    return n1 + n2;
}
