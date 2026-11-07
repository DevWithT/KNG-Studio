#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "RackRow.h"

/** Every instrument in one place, editing the current pattern. */
class RackView : public juce::Component,
                 private juce::Timer
{
public:
    explicit RackView (EditSession& sessionToUse);
    ~RackView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuild();
    void layoutRows();
    void syncToolbar();
    juce::Array<tracktion::Track*> getRackTracks();

    EditSession& session;

    juce::Label patternLabel { {}, "Pattern" }, barsLabel { {}, "Bars" }, hintLabel;
    juce::ComboBox patternBox, barsBox;
    juce::TextButton placeButton { "Place at Playhead" }, loopButton { "Loop Pattern" };
    juce::TextButton addDrumButton { "+ Drums" }, addSamplerButton { "+ Sampler" }, addInstrumentButton { "+ Instrument" };

    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<RackRow> rows;

    juce::Array<tracktion::Track*> builtTracks;
    juce::String lastLayoutSignature;
    int builtPatternCount = 0;

    static constexpr int toolbarHeight = 40;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackView)
};
