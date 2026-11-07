#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Every volume fader and knob: -inf to +12 dB, 0 dB in a comfortable spot, double-click for 0. */
namespace KngVolume
{
    inline void setupSlider (juce::Slider& s)
    {
        s.setTextValueSuffix ({});
        s.setRange (-60.0, 12.0, 0.1);
        s.setSkewFactorFromMidPoint (-6.0);
        s.setDoubleClickReturnValue (true, 0.0);

        s.textFromValueFunction = [] (double v)
        {
            if (v <= -59.9)
                return juce::String ("-inf");

            return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " dB";
        };

        s.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.trim().startsWithIgnoreCase ("-inf") ? -60.0 : text.getDoubleValue();
        };
    }
}
