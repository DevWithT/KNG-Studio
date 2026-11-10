#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "KngPresets.h"

class KngVerbPlugin;

/** KNG Verb panel: picture of the tail, the wet EQ range, a level meter, knobs and presets. */
class KngVerbView : public tracktion::Plugin::EditorComponent,
                    private juce::Timer
{
public:
    explicit KngVerbView (KngVerbPlugin& owner);
    ~KngVerbView() override;

    bool allowWindowResizing() override                               { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        int param = 0;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
    };

    void timerCallback() override;
    void beginEdit();
    float value (int p) const;
    void setValue (int p, float v);
    void drawTail (juce::Graphics&, juce::Rectangle<float> area);
    void drawBand (juce::Graphics&, juce::Rectangle<float> area);

    KngVerbPlugin& plugin;
    tracktion::Plugin::Ptr pluginRef;

    juce::Label titleLabel;
    KngPresetMenu presetBox;
    std::vector<Knob> knobs;

    juce::Rectangle<float> tailArea, bandArea, meterArea;
    float wetDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngVerbView)
};
