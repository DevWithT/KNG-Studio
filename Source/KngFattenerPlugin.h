#pragma once

#include "KngFx.h"
#include "KngFilters.h"
#include <array>

/** KNG Fattener: bigger 808s and kicks. Sub boost, an octave-down layer, punch, drive, tone, clip, mono lows. */
class KngFattenerPlugin : public KngFxBase
{
public:
    enum Param { sub, octave, punch, drive, tone, clip, monoLows, output, mix, fat };

    KngFattenerPlugin (tracktion::PluginCreationInfo);
    ~KngFattenerPlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override        { return "KNG Fattener"; }
    juce::String getPluginType() override        { return xmlTypeName; }
    juce::String getHint() const override        { return "Bigger 808s and kicks. Drive and Clip help them cut through on phone speakers."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;
    void drawDisplay (juce::Graphics&, juce::Rectangle<float>, const KngHistory&) override;

    /** The Fattener's own panel. */
    std::unique_ptr<EditorComponent> createEditor() override;

    /** The drive and clip curve at an input level from 0 to 1, for the panel. */
    float curveAt (float x);

private:
    std::array<KngDsp::Biquad, 2> subShelf, toneShelf, monoSplit;
    KngDsp::Biquad octIn1, octIn2, octOut1, octOut2;
    KngDsp::Follower fastEnv, slowEnv, octEnv;
    float flip = 1.0f;
    bool above = false;
};
