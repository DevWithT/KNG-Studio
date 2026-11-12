#pragma once

#include "KngFx.h"
#include "KngFonts.h"

/** One plugin's colors and fonts. Every KNG plugin face has its own. */
struct KngFaceStyle
{
    juce::Colour accent, bodyLight, bodyDark, pointer, track, label, value, panel, outline;
    KngFonts::Face labelFace = KngFonts::barlowSemi, valueFace = KngFonts::mono, menuFace = KngFonts::mono;
    bool upperCaseLabels = true;
};

/** A parameter's value as text: 45%, 1.20 s, -3.0 dB, +7 st, 2.4 kHz... */
juce::String kngFormat (const KngParamDef& def, float value);

/** Knobs, menus and buttons drawn in one plugin's colors. */
class KngFaceLook : public juce::LookAndFeel_V4
{
public:
    explicit KngFaceLook (const KngFaceStyle& styleToUse);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool isMouseOver, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    const KngFaceStyle style;
};

/** A knob bound to one parameter, with its name and live value underneath. */
class KngFaceKnob : public juce::Component,
                    private juce::Timer
{
public:
    KngFaceKnob (KngFxBase& plugin, int paramIndex, const KngFaceStyle& styleToUse, const juce::String& labelText = {});
    KngFaceKnob (tracktion::Plugin& plugin, tracktion::AutomatableParameter& parameter, const KngParamDef& defToUse,
                 const KngFaceStyle& styleToUse, const juce::String& labelText = {});
    ~KngFaceKnob() override;

    void setArcColour (juce::Colour c)   { knob.setColour (juce::Slider::rotarySliderFillColourId, c); }

    void paint (juce::Graphics&) override;
    void resized() override;

    float labelSize = 13.0f, valueSize = 12.0f;
    juce::Colour labelColour;

private:
    void timerCallback() override;

    tracktion::Plugin::Ptr pluginRef;
    tracktion::AutomatableParameter::Ptr param;
    const KngParamDef def;
    const KngFaceStyle style;
    juce::String label;
    juce::Slider knob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngFaceKnob)
};

/** An on/off switch with a glowing light, bound to one parameter. */
class KngFaceToggle : public juce::Component,
                      public juce::SettableTooltipClient,
                      private juce::Timer
{
public:
    KngFaceToggle (KngFxBase& plugin, int paramIndex, const KngFaceStyle& styleToUse, const juce::String& labelText);
    KngFaceToggle (tracktion::Plugin& plugin, tracktion::AutomatableParameter& parameter,
                   const KngFaceStyle& styleToUse, const juce::String& labelText);
    ~KngFaceToggle() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override   { repaint(); }

    tracktion::Plugin::Ptr pluginRef;
    tracktion::AutomatableParameter::Ptr param;
    const KngFaceStyle style;
    juce::String label;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngFaceToggle)
};

/** A row of pills for a parameter with named choices (Saw | Square | ...). Click one to pick it. */
class KngFaceChoice : public juce::Component,
                      public juce::SettableTooltipClient,
                      private juce::Timer
{
public:
    KngFaceChoice (tracktion::Plugin& plugin, tracktion::AutomatableParameter& parameter, const KngParamDef& defToUse,
                   const KngFaceStyle& styleToUse, const juce::String& shortNames = {});
    ~KngFaceChoice() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    juce::Colour onColour, onTextColour { 0xff081018 };

private:
    void timerCallback() override   { repaint(); }
    int getSelected() const;

    tracktion::Plugin::Ptr pluginRef;
    tracktion::AutomatableParameter::Ptr param;
    const KngParamDef def;
    const KngFaceStyle style;
    juce::StringArray names;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngFaceChoice)
};

/** The small arrow button that pops a panel out into its own window. */
class KngPopButton : public juce::Button
{
public:
    KngPopButton() : juce::Button ("Pop out")   {}

    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
};
