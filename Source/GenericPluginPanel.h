#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

/** A panel of sliders for built-in effects, which don't have their own UI. */
class GenericPluginPanel : public juce::Component,
                           private juce::Timer
{
public:
    explicit GenericPluginPanel (tracktion::Plugin& pluginToEdit);
    ~GenericPluginPanel() override;

    /** Pops the panel up next to a button or row. It closes when you click away. */
    static void showFor (tracktion::Plugin& plugin, juce::Component& anchor);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Row
    {
        tracktion::AutomatableParameter* param = nullptr;
        std::unique_ptr<juce::Label> name, value;
        std::unique_ptr<juce::Slider> slider;
    };

    void timerCallback() override;

    tracktion::Plugin::Ptr plugin;
    juce::Label titleLabel;
    juce::ToggleButton enabledButton { "On" };
    std::vector<Row> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenericPluginPanel)
};
