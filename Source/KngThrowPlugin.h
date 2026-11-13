#pragma once

#include "KngFx.h"
#include "KngFilters.h"
#include <array>
#include <atomic>
#include <functional>
#include <vector>

/** KNG Throw: an echo that stays out of the way while you sing and blooms in the gaps, locked to the song's tempo. */
class KngThrowPlugin : public KngFxBase
{
public:
    enum Param { time, feedback, duck, tone, wash, pingPong, mix };

    KngThrowPlugin (tracktion::PluginCreationInfo);
    ~KngThrowPlugin() override;

    static const char* xmlTypeName;
    juce::String getName() const override   { return "KNG Throw"; }
    juce::String getPluginType() override   { return xmlTypeName; }
    juce::String getHint() const override   { return "Echoes that hide while you sing and come out at the end of each line. Duck sets how much they hide."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;

    /** This effect's own face. */
    std::unique_ptr<EditorComponent> createEditor() override;

    static juce::StringArray timeNames()   { return { "1/16", "1/8", "1/8 Dot", "1/4", "1/4 Dot", "1/2" }; }
    static float beatsFor (int t);
    float getBpm() const   { return bpm.load(); }

private:
    struct Allpass
    {
        std::vector<float> buf;
        int pos = 0;

        void setSize (int n)   { buf.assign ((size_t) juce::jmax (1, n), 0.0f); pos = 0; }
        void clear()           { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }

        float process (float x, float g)
        {
            const float d = buf[(size_t) pos];
            const float y = d - g * x;
            buf[(size_t) pos] = x + g * y;

            if (++pos >= (int) buf.size())
                pos = 0;

            return y;
        }
    };

    // The song's tempo is read on the message thread, a few times a second
    struct BpmTimer : public juce::Timer
    {
        std::function<void()> onTick;
        void timerCallback() override   { if (onTick) onTick(); }
    };

    float readDelay (const std::vector<float>& buf, double delaySamples) const;

    double sampleRate = 44100.0;
    std::array<std::vector<float>, 2> lines;
    int writePos = 0;
    double currentDelay = 0.0;

    std::array<KngDsp::Biquad, 2> toneLp, toneHp;
    std::array<std::array<Allpass, 2>, 2> diffusers;
    float lastTone = -1.0f;
    double lastRate = 0.0;
    float duckEnv = 0.0f;

    std::atomic<float> bpm { 120.0f };
    BpmTimer bpmTimer;
};
