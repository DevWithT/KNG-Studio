#pragma once

#include "KngFace.h"
#include "KngPresets.h"

/** Builds a face's colors and fonts in one line. */
KngFaceStyle kngMakeStyle (juce::uint32 accent, juce::uint32 bodyLight, juce::uint32 bodyDark, juce::uint32 pointer,
                           juce::uint32 label, juce::uint32 value, juce::uint32 panel, juce::uint32 outline,
                           KngFonts::Face labelFace, bool upperCaseLabels);

/** The shared frame every KNG effect face is built on: its panel, title, header switches, presets,
    pop-out, a signature screen, an optional meter column and a row of knobs.
    Each face draws its own title and screen. */
class KngFaceView : public tracktion::Plugin::EditorComponent,
                    private juce::Timer
{
public:
    struct Palette { juce::Colour page, top, bottom, edge; };

    KngFaceView (tracktion::Plugin& pluginToShow, const KngFaceStyle& styleToUse, Palette paletteToUse);
    ~KngFaceView() override;

    bool allowWindowResizing() override                               { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    tracktion::Plugin& plugin;
    const KngFaceStyle style;
    const Palette palette;
    int titleWidth = 220, sideWidth = 0, maxKnobWidth = 96;
    juce::Rectangle<float> displayArea, sideArea;

    void attachPresets (const juce::StringArray& names, std::function<void (int)> apply);
    KngFaceKnob& addKnob (tracktion::AutomatableParameter& param, const KngParamDef& def, const juce::String& label);
    KngFaceKnob& addKnob (KngFxBase& fx, int index, const juce::String& label);
    void addHeaderChoice (tracktion::AutomatableParameter& param, float minValue, const juce::String& names, int width);
    void addHeaderSwitch (tracktion::AutomatableParameter& param, const juce::String& label, const juce::String& tip, int width);
    void clearKnobs();

    virtual void paintTitle (juce::Graphics&, juce::Rectangle<float> area) = 0;
    virtual void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) = 0;
    virtual void paintSide (juce::Graphics&, juce::Rectangle<float>) {}
    virtual void tick() {}

    /** Helpers the faces share */
    void drawTitle (juce::Graphics&, juce::Rectangle<float> area, const juce::String& word, KngFonts::Face face, float size) const;
    void drawScreen (juce::Graphics&, juce::Rectangle<float> area) const;
    void drawMeter (juce::Graphics&, juce::Rectangle<float> area, float db, const juce::String& label) const;
    void drawInOut (juce::Graphics&, juce::Rectangle<float> area, float inDb, float outDb) const;
    static float fallingDb (float gain, float previousDb);

private:
    void timerCallback() override;

    KngFaceLook look;
    tracktion::Plugin::Ptr pluginRef;
    KngPresetMenu presetBox;
    KngPopButton popButton;
    std::vector<std::unique_ptr<juce::Component>> knobs, headerItems;
    std::vector<int> headerWidths;
    juce::Rectangle<int> panel, header, titleArea;
    juce::String lastVisible;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngFaceView)
};
