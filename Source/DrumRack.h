#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

class DrumRackComponent : public juce::Component,
                          public juce::DragAndDropTarget,
                          public juce::FileDragAndDropTarget,
                          private juce::Timer
{
public:
    DrumRackComponent (EditSession& sessionToUse, tracktion::Track& trackToUse);
    ~DrumRackComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // Drags from the Browser
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    // Drags from Windows Explorer
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragMove (const juce::StringArray&, int x, int y) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;

private:
    void timerCallback() override   { repaint(); }
    int getTrackIndex();
    juce::Rectangle<int> getPadArea() const;
    juce::Rectangle<int> getPadBounds (int pad) const;
    int padAt (juce::Point<int>) const;

    void triggerPad (int pad);
    void selectPad (int pad);
    void dropFiles (const juce::Array<juce::File>& files, int pad);
    void showPadMenu (int pad);
    void loadKitFromChooser();

    EditSession& session;
    tracktion::Track::Ptr track;

    int selectedPad = 0;
    int hoverPad = -1;
    juce::uint32 hitTimes[16] {};

    juce::TextButton loadKitButton { "Load Kit..." }, clearButton { "Clear All" };
    juce::Label hintLabel, padNameLabel;
    juce::Label volLabel { {}, "Volume" }, panLabel { {}, "Pan" }, pitchLabel { {}, "Pitch" };
    juce::Slider volSlider, panSlider, pitchSlider;
    std::unique_ptr<juce::FileChooser> chooser;

    static constexpr int toolbarHeight = 36;
    static constexpr int editorHeight  = 116;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumRackComponent)
};

//==============================================================================
class DrumRackWindow : public juce::DocumentWindow
{
public:
    DrumRackWindow (EditSession& session, tracktion::Track& track);

    void closeButtonPressed() override          { setVisible (false); }
    tracktion::Track* getTrack() const          { return track.get(); }

private:
    tracktion::Track::Ptr track;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumRackWindow)
};

/** The drum panel (Sequencer + Pads tabs) for use inside the dock. */
std::unique_ptr<juce::Component> createDrumRackPanel (EditSession& session, tracktion::Track& track);
