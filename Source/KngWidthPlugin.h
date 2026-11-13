#pragma once

#include "KngFx.h"
#include "KngFilters.h"
#include <array>
#include <atomic>
#include <vector>

/** KNG Width: makes a sound wider or narrower, keeps the low end in the middle, and Haas can spread a mono vocal. */
class KngWidthPlugin : public KngFxBase
{
public:
    enum Param { width, monoBass, haas, output };

    KngWidthPlugin (tracktion::PluginCreationInfo);
    ~KngWidthPlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override   { return "KNG Width"; }
    juce::String getPluginType() override   { return xmlTypeName; }
    juce::String getHint() const override   { return "Widens or narrows the stereo image. On a mono vocal, turn up Haas to spread it."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;

    /** This effect's own face. */
    std::unique_ptr<EditorComponent> createEditor() override;

    /** Recent left/right samples for the face's scope, and how alike the two sides are (-1 to +1). */
    static constexpr int scopeSize = 512;
    float getScopeL (int i) const   { return scopeL[(size_t) i].load (std::memory_order_relaxed); }
    float getScopeR (int i) const   { return scopeR[(size_t) i].load (std::memory_order_relaxed); }
    int getScopePos() const          { return scopePos.load(); }
    float getCorrelation() const     { return correlation.load(); }

private:
    double sampleRate = 44100.0;
    KngDsp::Biquad sideLow;
    float lastMonoBass = -1.0f;
    double lastRate = 0.0;

    std::vector<float> haasBuf;
    int haasPos = 0;

    std::array<std::atomic<float>, scopeSize> scopeL {}, scopeR {};
    std::atomic<int> scopePos { 0 };
    std::atomic<float> correlation { 1.0f };
    int scopeSkip = 0;
};
