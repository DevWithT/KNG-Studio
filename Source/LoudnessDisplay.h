#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** Master loudness readout: momentary, short-term, integrated (LUFS) and peak. Click to reset. */
class LoudnessDisplay : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    explicit LoudnessDisplay (EditSession& sessionToUse);
    ~LoudnessDisplay() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    static double toLufs (double meanSquare);
    double average (int lastN) const;
    double integrated() const;

    EditSession& session;
    std::vector<float> blocks;
    double momentary = -100.0, shortTerm = -100.0, whole = -100.0;
    float peakDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessDisplay)
};
