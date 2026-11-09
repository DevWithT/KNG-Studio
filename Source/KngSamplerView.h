#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "EditSession.h"

class KngSamplerPlugin;

/** Panel for the KNG Sampler: waveform with loop handles, settings, and a keyboard. */
class KngSamplerView : public juce::Component,
                       public juce::DragAndDropTarget,
                       public juce::FileDragAndDropTarget,
                       private juce::MidiKeyboardState::Listener,
                       private juce::Timer
{
public:
    KngSamplerView (EditSession& sessionToUse, tracktion::Track& trackToUse);
    ~KngSamplerView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

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
    void handleNoteOn (juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int, int, float) override;

    KngSamplerPlugin* getPlugin();
    int getTrackIndex();
    void loadFile (const juce::File& file);
    void chooseFile();
    void set (const juce::Identifier& id, const juce::var& value);
    void syncControls();
    void rebuildPeaks();
    void setupKnob (juce::Slider& s, double min, double max, double mid, const juce::String& suffix,
                    const juce::Identifier& id);

    juce::Rectangle<int> getWaveArea() const;
    int fracToX (float f) const;
    float xToFrac (int x) const;

    EditSession& session;
    tracktion::Track::Ptr track;

    juce::TextButton loadButton { "Load Sample..." };
    juce::Label nameLabel, tipLabel;
    juce::Label rootLabel { {}, "Root" }, gainLabel { {}, "Volume" }, attackLabel { {}, "Attack" },
                releaseLabel { {}, "Release" }, glideLabel { {}, "Glide" };
    juce::ComboBox rootBox;
    juce::Slider gainSlider, attackSlider, releaseSlider, glideSlider;
    juce::ToggleButton monoButton { "Mono (808)" }, loopButton { "Sustain loop" }, oneShotButton { "One-shot" };

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<float> peaks;
    juce::File peaksFile;
    int peaksWidth = 0;
    bool hovering = false;
    int dragHandle = 0;   // 1 = loop start, 2 = loop end

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngSamplerView)
};
