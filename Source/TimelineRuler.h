#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

class TimelineRuler : public juce::Component,
                      public juce::SettableTooltipClient,
                      private juce::Timer
{
public:
    explicit TimelineRuler (EditSession& sessionToUse);
    ~TimelineRuler() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override   { repaint(); }
    double xToTime (int x) const;
    int timeToX (double seconds) const;
    double snapToBeat (double seconds) const;

    EditSession& session;
    bool draggingLoop = false;
    double dragStartTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineRuler)
};
