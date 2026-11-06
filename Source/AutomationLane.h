#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

//==============================================================================
/** Left side of an automation row: pick what to automate. */
class AutomationHeader : public juce::Component,
                         private juce::Timer
{
public:
    AutomationHeader (EditSession& sessionToUse, int trackIndexToUse);
    ~AutomationHeader() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override   { refreshList(); }
    void refreshList();

    EditSession& session;
    const int trackIndex;

    juce::Label label { {}, "Automation" };
    juce::ComboBox paramBox;
    juce::TextButton clearButton { "Clear" };
    juce::StringArray lastIds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationHeader)
};

//==============================================================================
/** Right side of an automation row: the curve itself. */
class AutomationLane : public juce::Component,
                       private juce::Timer
{
public:
    AutomationLane (EditSession& sessionToUse, int trackIndexToUse);
    ~AutomationLane() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override   { repaint(); }
    int valueToY (float value, juce::Range<float> range) const;
    float yToValue (int y, juce::Range<float> range) const;
    int findPointAt (tracktion::AutomationCurve&, juce::Point<int>, juce::Range<float>);

    EditSession& session;
    const int trackIndex;
    int dragIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationLane)
};
