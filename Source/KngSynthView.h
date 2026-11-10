#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "EditSession.h"
#include "KngPresets.h"

class KngSynthPlugin;

/** KNG Synth panel: presets, every knob grouped by section, and a keyboard. */
class KngSynthView : public juce::Component,
                     private juce::MidiKeyboardState::Listener,
                     private juce::Timer
{
public:
    KngSynthView (EditSession& sessionToUse, tracktion::Track& trackToUse);
    ~KngSynthView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        int param = 0;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> knob;
        std::unique_ptr<juce::ComboBox> box;
    };

    struct SectionBox
    {
        juce::String name;
        juce::Rectangle<int> bounds;
        std::vector<int> controls;
    };

    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override   { if (onPaint) onPaint (g); }
    };

    void timerCallback() override;
    void handleNoteOn (juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int, int, float) override;
    void buildControls();
    void layoutContent (int width);

    EditSession& session;
    tracktion::Track::Ptr track;
    tracktion::Plugin::Ptr pluginRef;
    KngSynthPlugin* plugin = nullptr;

    juce::Label titleLabel;
    KngPresetMenu presetBox;
    juce::Viewport viewport;
    Content content;

    std::vector<Control> controls;
    std::vector<SectionBox> sections;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngSynthView)
};
