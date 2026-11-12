#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** Column listing every pattern. Click to edit it in the Rack, the arrow loops it. */
class PatternListPanel : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::Timer
{
public:
    explicit PatternListPanel (EditSession& sessionToUse);
    ~PatternListPanel() override;

    /** Set by the main window: show the Rack when a pattern is picked. */
    std::function<void()> onPatternPicked;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<int> rowBounds (int pattern);
    int patternAt (int y);
    void rename (int pattern);
    void showMenu (int pattern);

    EditSession& session;
    juce::TextButton newButton { "+ New pattern" };
    int lastCount = -1;

    static constexpr int headerHeight = 30;
    static constexpr int rowHeight = 30;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternListPanel)
};
