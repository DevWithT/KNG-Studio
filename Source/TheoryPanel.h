#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Right-hand panel in the piano roll that explains what you're looking at. */
class TheoryPanel : public juce::Component
{
public:
    TheoryPanel();

    void update (int keyRoot, int scaleIndex, int chordRoot, int chordType, int hoverNote);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::TextEditor text;
    juce::String lastText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TheoryPanel)
};
