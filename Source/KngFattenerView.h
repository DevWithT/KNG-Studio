#pragma once

#include "KngFattenerPlugin.h"
#include "KngFace.h"
#include "KngPresets.h"

/** KNG Fattener's face: dark red, one big FAT knob, LOW on the left, HIT on the right. */
class KngFattenerView : public tracktion::Plugin::EditorComponent,
                        private juce::Timer
{
public:
    explicit KngFattenerView (KngFattenerPlugin& owner);
    ~KngFattenerView() override;

    bool allowWindowResizing() override                               { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintCurve (juce::Graphics&, juce::Rectangle<float> area);
    void paintMeter (juce::Graphics&, juce::Rectangle<float> row, const juce::String& label, float db);

    KngFattenerPlugin& plugin;
    tracktion::Plugin::Ptr pluginRef;

    KngFaceStyle style, fatStyle;
    KngFaceLook look;

    KngPresetMenu presetBox;
    KngPopButton popButton;
    std::unique_ptr<KngFaceKnob> fatKnob, subKnob, octaveKnob, punchKnob, clipKnob, driveKnob, toneKnob, outputKnob, mixKnob;
    std::unique_ptr<juce::Component> monoSwitch;

    juce::Rectangle<int> panel, header, lowLabel, hitLabel, hintArea, curveBox, meterArea;
    int stripDividerX = 0;
    float inDb = -100.0f, outDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngFattenerView)
};
