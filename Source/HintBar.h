#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "KngTheme.h"

// Status bar hint: shows the tooltip text of whatever the mouse is over, right away (no waiting for a pop-up).
class HintBar : public juce::Label,
                private juce::Timer
{
public:
    HintBar()
    {
        setFont (juce::Font (juce::FontOptions (11.5f)));
        setColour (juce::Label::textColourId, KngTheme::text);
        setJustificationType (juce::Justification::centredLeft);
        setInterceptsMouseClicks (false, false);
        startTimerHz (10);
    }

    ~HintBar() override { stopTimer(); }

private:
    void timerCallback() override
    {
        juce::String hint;
        auto* under = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();

        // Walk up from the component under the mouse to the first one that has a tooltip
        for (auto* c = under; c != nullptr && hint.isEmpty(); c = c->getParentComponent())
            if (auto* tip = dynamic_cast<juce::TooltipClient*> (c))
                hint = tip->getTooltip();

        if (getText() != hint)
            setText (hint, juce::dontSendNotification);
    }
};
