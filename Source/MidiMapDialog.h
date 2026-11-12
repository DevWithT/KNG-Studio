#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** Lists every MIDI controller link in the song, with Remove. */
class MidiMapDialog : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::Timer
{
public:
    explicit MidiMapDialog (EditSession& sessionToUse);
    ~MidiMapDialog() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void timerCallback() override;

    EditSession& session;
    juce::Label hint;
    juce::ListBox list;
    juce::TextButton learnButton { "MIDI Learn" }, removeButton { "Remove Selected" };
    int lastCount = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMapDialog)
};
