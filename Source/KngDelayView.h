#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "KngPresets.h"

class KngDelayPlugin;

/** KNG Delay panel: echo timeline, sync and note lengths, knobs and presets. */
class KngDelayView : public tracktion::Plugin::EditorComponent,
                     private juce::Timer
{
public:
    explicit KngDelayView (KngDelayPlugin& owner);
    ~KngDelayView() override;

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
    Knob makeKnob (int param);
    void drawEchoes (juce::Graphics&, juce::Rectangle<float> area);

    KngDelayPlugin& plugin;
    tracktion::Plugin::Ptr pluginRef;

    juce::Label titleLabel;
    KngPresetMenu presetBox;
    juce::ComboBox modeBox, divLBox, divRBox;
    juce::ToggleButton syncButton { "Sync to tempo" };
    juce::Label leftLabel { {}, "Left" }, rightLabel { {}, "Right" };

    Knob timeLKnob, timeRKnob;
    std::vector<Knob> knobs;

    juce::Rectangle<float> echoArea, meterArea;
    float wetDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngDelayView)
};
