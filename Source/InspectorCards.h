#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "KngFace.h"

/** How a plugin looks around the app: its color, its chip, and the knob style for its card. */
struct KngIdentity
{
    juce::Colour accent, top, bottom;
    juce::String chip;
    KngFonts::Face chipFace = KngFonts::spaceGrotesk;
    KngFaceStyle style;
};

KngIdentity kngIdentityFor (tracktion::Plugin& plugin);

/** A plugin's color chip, like the red F for the Fattener. */
void kngDrawChip (juce::Graphics&, juce::Rectangle<float> area, const KngIdentity&, float alpha = 1.0f);

/** The inspector's instrument card, in the instrument's own colors, with a mini screen and its main knobs. */
class InstrumentCard : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer
{
public:
    InstrumentCard (EditSession& sessionToUse, tracktion::Plugin& instrument);
    ~InstrumentCard() override;

    std::function<void()> onOpen, onChange;

    int getIdealHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    enum Kind { generic, eight08, synth, sampler };

    void timerCallback() override   { repaint (displayArea); }
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area);
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area);

    EditSession& session;
    tracktion::Plugin::Ptr plugin;
    Kind kind = generic;

    KngIdentity id;
    KngFaceLook look;

    KngPopButton popButton;
    juce::TextButton changeButton { "Change" };
    std::vector<std::unique_ptr<juce::Component>> knobs;

    juce::Rectangle<int> titleArea, displayArea;
    std::vector<float> peaks;
    const void* peaksSource = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentCard)
};
