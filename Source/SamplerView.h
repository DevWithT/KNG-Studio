#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "EditSession.h"

/** KNG Sampler: one sample played across the keyboard. */
class SamplerComponent : public juce::Component,
                         public juce::DragAndDropTarget,
                         public juce::FileDragAndDropTarget,
                         private juce::MidiKeyboardState::Listener,
                         private juce::Timer
{
public:
    SamplerComponent (EditSession& sessionToUse, tracktion::Track& trackToUse);
    ~SamplerComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Drags from the Browser
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override   { hovering = true;  repaint(); }
    void itemDragExit (const SourceDetails&) override    { hovering = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    // Drags from Windows Explorer
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override   { hovering = true;  repaint(); }
    void fileDragExit (const juce::StringArray&) override              { hovering = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void timerCallback() override   { syncFromSampler(); }
    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

    int getTrackIndex();
    void loadFile (const juce::File& file);
    void syncFromSampler();
    juce::Rectangle<int> getDropArea() const;

    EditSession& session;
    tracktion::Track::Ptr track;

    juce::TextButton loadButton { "Load Sample..." };
    juce::Label sampleLabel, tipLabel;
    juce::Label rootLabel { {}, "Root note" }, gainLabel { {}, "Volume" };
    juce::ComboBox rootBox;
    juce::Slider gainSlider;
    juce::ToggleButton oneShotButton { "One-shot (play the whole sample)" };

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    std::unique_ptr<juce::FileChooser> chooser;
    bool hovering = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerComponent)
};

//==============================================================================
class SamplerWindow : public juce::DocumentWindow
{
public:
    SamplerWindow (EditSession& session, tracktion::Track& track);

    void closeButtonPressed() override          { setVisible (false); }
    tracktion::Track* getTrack() const          { return track.get(); }

private:
    tracktion::Track::Ptr track;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerWindow)
};
