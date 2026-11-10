#pragma once

#include <tracktion_engine/tracktion_engine.h>

class KngCompPlugin;

/** KNG Comp panel: curve graph, scrolling gain reduction, knobs, modes and presets. */
class KngCompView : public tracktion::Plugin::EditorComponent,
                    private juce::Timer
{
public:
    explicit KngCompView (KngCompPlugin& pluginToEdit);
    ~KngCompView() override;

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
    void drawTransfer (juce::Graphics&, juce::Rectangle<float> area);
    void drawHistory (juce::Graphics&, juce::Rectangle<float> area);

    KngCompPlugin& plugin;
    tracktion::Plugin::Ptr pluginRef;

    juce::Label titleLabel;
    juce::ComboBox presetBox, modeBox;
    juce::ToggleButton autoButton { "Auto Gain" };
    std::vector<Knob> knobs;

    juce::Rectangle<float> transferArea, historyArea;
    std::vector<float> grHistory = std::vector<float> (360, 0.0f);
    std::vector<float> inHistory = std::vector<float> (360, -100.0f);
    int historyPos = 0;
    float inDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngCompView)
};
