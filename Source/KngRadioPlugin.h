#pragma once

#include "KngFx.h"
#include "KngFilters.h"
#include <array>

/** KNG Radio: phone, old radio, megaphone and walkie-talkie sounds for intros, ad-libs and breakdowns. */
class KngRadioPlugin : public KngFxBase
{
public:
    enum Param { band, tone, drive, crush, noise, mix };

    KngRadioPlugin (tracktion::PluginCreationInfo);
    ~KngRadioPlugin() override   { notifyListenersOfDeletion(); }

    static const char* xmlTypeName;
    juce::String getName() const override   { return "KNG Radio"; }
    juce::String getPluginType() override   { return xmlTypeName; }
    juce::String getHint() const override   { return "Pick a sound up top. Tone slides the band up or down, Crush and Noise make it older."; }

    void initialise (const tracktion::PluginInitialisationInfo&) override;
    void reset() override;
    void applyToBuffer (const tracktion::PluginRenderContext&) override;

    /** This effect's own face. */
    std::unique_ptr<EditorComponent> createEditor() override;

    /** The band that gets through right now, in Hz (for the face's dial). */
    void getBandEdges (float& lowHz, float& highHz) const;

    static juce::StringArray bandNames()   { return { "Phone", "Radio", "Megaphone", "Walkie" }; }

private:
    struct Shape { float low, high, bellHz, bellDb; };
    static Shape shapeFor (int b);

    double sampleRate = 44100.0;
    std::array<std::array<KngDsp::Biquad, 5>, 2> filters;
    std::array<float, 2> held {}, holdCount {};
    float lastBand = -1.0f, lastTone = -100.0f;
    double lastRate = 0.0;
    juce::Random random;
};
