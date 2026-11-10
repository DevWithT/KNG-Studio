#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include <atomic>
#include <utility>
#include <vector>

/** One control on a KNG effect. choices: nullptr for a knob, "A|B|C" for a drop-down, "toggle" for a switch.
    unit: "dB", "Hz", "ms", "ct", "%" or "" (shapes how the value reads). */
struct KngParamDef
{
    const char* id;
    const char* name;
    float min, max, def, mid, step;
    const char* unit;
    const char* choices;
};

struct KngPresetDef
{
    const char* name;
    std::vector<std::pair<int, float>> values;
};

/** Recent meter readings for the panel picture. The newest is at pos - 1. */
struct KngHistory
{
    std::vector<float> in, out, gr;
    int pos = 0;
};

/** Shared base for smaller KNG effects: parameters, presets, meters and a ready-made panel.
    Each effect must call notifyListenersOfDeletion() in its own destructor. */
class KngFxBase : public tracktion::Plugin
{
public:
    KngFxBase (tracktion::PluginCreationInfo, std::vector<KngParamDef>, std::vector<KngPresetDef>);
    ~KngFxBase() override;

    juce::String getSelectableDescription() override   { return getName(); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }
    void deinitialise() override {}

    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override
    {
        getLeftRightChannelNames (ins, outs);
    }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;
    std::unique_ptr<EditorComponent> createEditor() override;

    int getNumParams() const                                  { return (int) defs.size(); }
    const KngParamDef& getDef (int i) const                   { return defs[(size_t) i]; }
    tracktion::AutomatableParameter* getParam (int i) const   { return params[(size_t) i].get(); }
    const std::vector<KngPresetDef>& getPresets() const       { return presets; }
    void applyPreset (int index);
    void takeMeters (float& in, float& out, float& reductionDb);

    /** Each effect draws its own picture in the panel. */
    virtual void drawDisplay (juce::Graphics&, juce::Rectangle<float>, const KngHistory&) {}
    virtual juce::String getHint() const   { return {}; }

protected:
    float get (int i) const   { return params[(size_t) i]->getCurrentValue(); }
    void reportMeters (float in, float out, float reductionDb);

private:
    std::vector<KngParamDef> defs;
    std::vector<KngPresetDef> presets;

    // Counted references keep each parameter alive until this plugin is done with it
    std::vector<tracktion::AutomatableParameter::Ptr> params;
    juce::OwnedArray<juce::CachedValue<float>> values;

    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, grPeak { 0.0f };
};

/** Scrolling gain reduction (red from the top) over the input level, with a live readout. */
inline void kngDrawHistory (juce::Graphics& g, juce::Rectangle<float> area, const KngHistory& h, float maxReductionDb)
{
    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (area, 6.0f);

    const int n = (int) h.gr.size();

    if (n == 0)
        return;

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (area.toNearestInt());

    const float colW = area.getWidth() / (float) n;

    for (int k = 0; k < n; ++k)
    {
        const int idx = (h.pos + k) % n;
        const float x = area.getX() + k * colW;

        const float inH = juce::jlimit (0.0f, 1.0f, (h.in[(size_t) idx] + 60.0f) / 60.0f) * area.getHeight();
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (x, area.getBottom() - inH, colW + 1.0f, inH);

        const float grH = juce::jlimit (0.0f, 1.0f, h.gr[(size_t) idx] / maxReductionDb) * area.getHeight();
        g.setColour (juce::Colour (0xffe6503a).withAlpha (0.6f));
        g.fillRect (x, area.getY(), colW + 1.0f, grH);
    }

    const float now = h.gr[(size_t) ((h.pos + n - 1) % n)];
    g.setColour (now > 0.05f ? juce::Colour (0xffe6503a) : juce::Colours::white.withAlpha (0.4f));
    g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    g.drawText ((now > 0.05f ? "-" : "") + juce::String (now, 1) + " dB",
                area.reduced (10.0f).toNearestInt(), juce::Justification::topRight);
}
