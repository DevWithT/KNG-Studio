#pragma once

#include "KngFx.h"
#include "KngPresets.h"

/** The panel every KngFxBase effect gets: presets, controls built from its parameters,
    its own picture, and in/out meters. */
class KngFxView : public tracktion::Plugin::EditorComponent,
                  private juce::Timer
{
public:
    explicit KngFxView (KngFxBase& owner);
    ~KngFxView() override;

    bool allowWindowResizing() override                               { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        int param = 0;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> knob;
        std::unique_ptr<juce::ComboBox> box;
        std::unique_ptr<juce::ToggleButton> toggle;
    };

    void timerCallback() override;
    void beginEdit();
    void drawMeter (juce::Graphics&, juce::Rectangle<float> area, float db, const juce::String& label);

    KngFxBase& plugin;
    tracktion::Plugin::Ptr pluginRef;

    juce::Label titleLabel, hintLabel;
    KngPresetMenu presetBox;
    std::vector<Control> controls;

    juce::Rectangle<float> displayArea, inMeterArea, outMeterArea;
    KngHistory history;
    float inDb = -100.0f, outDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngFxView)
};
