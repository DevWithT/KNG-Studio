#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** A small "G" tab on the name band of a clip that belongs to a group. */
namespace KngGroupBadge
{
    inline void draw (juce::Graphics& g, juce::Rectangle<int> clipArea)
    {
        if (clipArea.getWidth() < 44)
            return;

        const juce::Rectangle<float> tab ((float) clipArea.getRight() - 19.0f, (float) clipArea.getY() + 2.0f, 16.0f, 10.0f);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (tab, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawText ("G", tab.toNearestInt(), juce::Justification::centred);
    }
}
