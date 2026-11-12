#pragma once

#include "Kng808Plugin.h"
#include "KngFace.h"
#include "KngPresets.h"

/** KNG 808's face: black hardware, orange glow, a screen with the pitch drop and volume fade. */
class Kng808View : public tracktion::Plugin::EditorComponent,
                   private juce::Timer
{
public:
    explicit Kng808View (Kng808Plugin& owner);
    ~Kng808View() override;

    bool allowWindowResizing() override                               { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintScreen (juce::Graphics&, juce::Rectangle<float> area);
    void paintKeys (juce::Graphics&, juce::Rectangle<float> area);

    Kng808Plugin& plugin;
    tracktion::Plugin::Ptr pluginRef;

    KngFaceStyle style;
    KngFaceLook look;

    KngPresetMenu presetBox;
    KngPopButton popButton;
    std::unique_ptr<KngFaceKnob> lengthKnob, driveKnob, tuneKnob, dropKnob, dropTimeKnob, punchKnob, glideKnob, levelKnob;
    std::unique_ptr<KngFaceToggle> holdSwitch;

    juce::Rectangle<int> panel, header, screen, keysArea;
    int dividerY = 0, shownNote = -2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Kng808View)
};
