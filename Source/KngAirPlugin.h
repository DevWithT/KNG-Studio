#pragma once

#include "KngFx.h"
#include "KngFilters.h"
#include <array>

/** KNG Air: opens up the top of a vocal. A smooth air shelf, a presence lift, and Shine for a little sparkle. */
class KngAirPlugin : public KngFxBase
{
public:
    enum Param { air, freq, presence, shine, output };

    KngAirPlugin (tracktion::PluginCreationInfo);
    ~KngAirPlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override   { return "KNG Air"; }
    juce::String getPluginType() override   { return xmlTypeName; }
    juce::String getHint() const override   { return "Adds air and shine to the top of a vocal. Start with Air, then add Shine to taste."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;

    /** This effect's own face. */
    std::unique_ptr<EditorComponent> createEditor() override;

    /** Roughly how much the sound is lifted at a frequency, in dB (for the face's curve). */
    float curveAt (float hz) const;

private:
    double sampleRate = 44100.0;
    std::array<KngDsp::Biquad, 2> shelf, bell, shineHp;
    float lastAir = -100.0f, lastFreq = -1.0f, lastPresence = -100.0f;
    double lastRate = 0.0;
};
