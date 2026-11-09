#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "RackRow.h"

/** Every sound and instrument in one place, editing the current pattern. */
class RackView : public juce::Component,
                 public juce::DragAndDropTarget,
                 public juce::FileDragAndDropTarget,
                 private juce::Timer
{
public:
    explicit RackView (EditSession& sessionToUse);
    ~RackView() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    // Samples dropped on the Rack become new channels
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override   { hovering = true;  repaint(); }
    void itemDragExit (const SourceDetails&) override    { hovering = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override   { hovering = true;  repaint(); }
    void fileDragExit (const juce::StringArray&) override              { hovering = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void timerCallback() override;
    void rebuild();
    void layoutRows();
    void syncToolbar();
    void addSamples (const juce::Array<juce::File>& files);
    juce::Array<tracktion::Track*> getRackTracks();

    EditSession& session;

    juce::Label patternLabel { {}, "Pattern" }, barsLabel { {}, "Bars" }, hintLabel;
    juce::ComboBox patternBox, barsBox;
    juce::TextButton placeButton { "Place at Playhead" }, loopButton { "Loop Pattern" };
    juce::TextButton addSampleButton { "+ Sample" }, addInstrumentButton { "+ Instrument" };
    juce::Label swingLabel { {}, "Swing" };
    juce::Slider swingSlider;

    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<RackRow> rows;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<tracktion::Track*> builtTracks;
    juce::String lastLayoutSignature;
    int builtPatternCount = 0;
    bool hovering = false;

    static constexpr int toolbarHeight = 40;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackView)
};
