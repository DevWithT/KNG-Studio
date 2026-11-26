#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "KngEqDsp.h"
#include <atomic>

/** Invisible meter at the end of the master chain. Measures loudness the BS.1770 way
    (K-weighted, 100 ms blocks); the display does the LUFS math. Audio passes through untouched. */
class KngLoudnessPlugin : public tracktion::Plugin
{
public:
    KngLoudnessPlugin (tracktion::PluginCreationInfo creationInfo) : tracktion::Plugin (creationInfo) {}
    ~KngLoudnessPlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;

    juce::String getName() const override                    { return "KNG Loudness"; }
    juce::String getPluginType() override                    { return xmlTypeName; }
    juce::String getSelectableDescription() override         { return getName(); }
    BusLayout getBusses() const override                     { return BusLayout::singleStereoInOut(); }

    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override
    {
        getLeftRightChannelNames (ins, outs);
    }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;

    /** Message thread: K-weighted mean-square of each finished 100 ms block. Returns how many. */
    int pullBlocks (float* dest, int maxBlocks);

    /** Message thread: the highest sample peak since the last call. */
    float takePeak()   { return peak.exchange (0.0f); }

    /** Message thread: copies the newest `num` samples (up to scopeSize) of the left and right master signal, oldest first.
        Returns a counter that goes up as audio arrives, so the caller can tell whether anything is new. */
    juce::uint32 copyRecent (float* left, float* right, int num) const
    {
        num = juce::jlimit (1, scopeSize, num);
        const auto end = scopeWrite.load (std::memory_order_acquire);

        for (int i = 0; i < num; ++i)
        {
            const auto idx = (size_t) ((end - (juce::uint32) num + (juce::uint32) i) & (juce::uint32) (scopeSize - 1));
            left[i] = scopeL[idx];
            right[i] = scopeR[idx];
        }

        return end;
    }

    double getScopeSampleRate() const   { return sampleRate; }

private:
    KngEq::Coefs shelfCoefs, highPassCoefs;
    std::array<std::array<KngEq::State, 2>, 2> states {};
    double acc = 0.0;
    int count = 0, blockLength = 4800;

    juce::AbstractFifo fifo { 2048 };
    std::vector<float> fifoData = std::vector<float> (2048, 0.0f);
    std::atomic<float> peak { 0.0f };

    static constexpr int scopeSize = 8192;   // a power of two
    std::array<float, (size_t) scopeSize> scopeL {}, scopeR {};
    std::atomic<juce::uint32> scopeWrite { 0 };
};
