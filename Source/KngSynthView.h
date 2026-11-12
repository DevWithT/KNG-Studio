#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "KngSynthPlugin.h"
#include "KngFace.h"
#include "KngPresets.h"
#include <map>

/** KNG Synth's face: navy and cyan, laid out left to right the way the sound travels. */
class KngSynthView : public tracktion::Plugin::EditorComponent,
                     private juce::MidiKeyboardState::Listener,
                     private juce::Timer
{
public:
    explicit KngSynthView (KngSynthPlugin& owner);
    ~KngSynthView() override;

    bool allowWindowResizing() override                               { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum Display { dispNone, dispWaveA, dispWaveB, dispFilter, dispAmp, dispFilterEnv, dispLfo };

    struct Card
    {
        juce::String name;
        juce::Colour colour;
        Display display = dispNone;
        juce::Rectangle<int> bounds, displayArea;
    };

    void timerCallback() override;
    void handleNoteOn (juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int, int, float) override;

    void addKnob (int param, const char* label, juce::Colour colour);
    std::unique_ptr<KngFaceChoice> makeChoice (int param, const char* shortNames, juce::Colour colour);
    void layoutCard (Card& card, juce::Rectangle<int> area, KngFaceChoice* choice, bool choiceInTitle,
                     std::initializer_list<int> params);
    float value (int param) const;

    void paintCard (juce::Graphics&, const Card&);
    void paintWave (juce::Graphics&, juce::Rectangle<float>, juce::Colour, int wave, int voices, float detune);
    void paintFilter (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void paintEnvelope (juce::Graphics&, juce::Rectangle<float>, juce::Colour, float a, float d, float s, float r);
    void paintLfo (juce::Graphics&, juce::Rectangle<float>, juce::Colour);

    KngSynthPlugin& plugin;
    tracktion::Plugin::Ptr pluginRef;

    KngFaceStyle style;
    KngFaceLook look;

    KngPresetMenu presetBox;
    KngPopButton popButton;
    std::map<int, std::unique_ptr<KngFaceKnob>> knobs;
    std::unique_ptr<KngFaceChoice> modeChoice, waveAChoice, waveBChoice, filterTypeChoice, lfoShapeChoice;

    std::vector<Card> cards;
    juce::Rectangle<int> panel, header;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngSynthView)
};
