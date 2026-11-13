#pragma once

#include "KngFx.h"
#include "KngFilters.h"
#include <atomic>

/** KNG Gate: closes between phrases so breaths, room noise and headphone bleed drop away. Soft mode just turns them down. */
class KngGatePlugin : public KngFxBase
{
public:
    enum Param { threshold, range, attack, hold, release, lowCut, mode };

    KngGatePlugin (tracktion::PluginCreationInfo);
    ~KngGatePlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override   { return "KNG Gate"; }
    juce::String getPluginType() override   { return xmlTypeName; }
    juce::String getHint() const override   { return "Set Threshold just above the noise between phrases. Soft mode turns the gaps down instead of off."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;

    /** This effect's own face. */
    std::unique_ptr<EditorComponent> createEditor() override;

    /** True while the gate is letting sound through. */
    bool isOpen() const   { return openNow.load(); }

private:
    double sampleRate = 44100.0;
    KngDsp::Biquad detectHp;
    float lastLowCut = -1.0f;
    double lastRate = 0.0;

    float detectEnv = 0.0f, gainDb = 0.0f;
    bool open = false;
    int holdLeft = 0;
    std::atomic<bool> openNow { false };
};
