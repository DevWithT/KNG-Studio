#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** A small knob bound to one plugin parameter. */
class ParamKnob : public juce::Component,
                  private juce::Timer
{
public:
    ParamKnob (EditSession& sessionToUse, tracktion::AutomatableParameter::Ptr paramToUse);
    ~ParamKnob() override;

    void resized() override;

private:
    void timerCallback() override;

    EditSession& session;
    tracktion::AutomatableParameter::Ptr param;
    juce::Slider knob;
    juce::Label label;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamKnob)
};

/** One effect in the inspector: on/off dot, name (click to open), right-click for more. */
class EffectRow : public juce::Component,
                  public juce::SettableTooltipClient,
                  private juce::Timer
{
public:
    EffectRow (EditSession& sessionToUse, tracktion::Plugin& pluginToUse);
    ~EffectRow() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    /** VSTs open their own window, KNG effects open in the bottom panel, other built-ins get a knob panel. */
    static void openPlugin (EditSession& session, tracktion::Plugin& plugin, juce::Component& anchor);

private:
    void timerCallback() override   { repaint(); }

    EditSession& session;
    tracktion::Plugin::Ptr plugin;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectRow)
};

/** A plugin's most useful knobs: cutoff, resonance, attack, release, else its first ones. */
juce::Array<tracktion::AutomatableParameter*> pickMainParams (tracktion::Plugin& plugin, int maxCount);
