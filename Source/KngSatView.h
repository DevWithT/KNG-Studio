#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "KngPresets.h"

class KngSatPlugin;

/** KNG Saturator panel: shape curve, in/out meters, flavor, knobs and presets. */
class KngSatView : public tracktion::Plugin::EditorComponent,
                   private juce::Timer
{
public:
    explicit KngSatView (KngSatPlugin& owner);
    ~KngSatView() override;

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
    void drawCurve (juce::Graphics&, juce::Rectangle<float> area);
    void drawMeter (juce::Graphics&, juce::Rectangle<float> area, float db, const juce::String& label);

    KngSatPlugin& plugin;
    tracktion::Plugin::Ptr pluginRef;

    juce::Label titleLabel;
    KngPresetMenu presetBox;
    juce::ComboBox modeBox;
    juce::ToggleButton autoButton { "Auto Gain" };
    std::vector<Knob> knobs;

    juce::Rectangle<float> curveArea, inMeterArea, outMeterArea;
    float inDb = -100.0f, outDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngSatView)
};
