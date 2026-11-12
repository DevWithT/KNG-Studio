#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "KngSamplerPlugin.h"
#include "KngFace.h"

class EditSession;

/** The Sampler's keyboard: the root key glows pink, the note playing glows purple.
    Right-click a key to make it the root. */
class KngSamplerKeys : public juce::MidiKeyboardComponent
{
public:
    KngSamplerKeys (juce::MidiKeyboardState& state, KngSamplerPlugin& owner, std::function<void (int)> onRootPicked);

    void drawWhiteNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    KngSamplerPlugin& plugin;
    std::function<void (int)> onRoot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngSamplerKeys)
};

/** KNG Sampler's face: the waveform is the instrument. Violet loop edges, a playhead, a keyboard with the root. */
class KngSamplerView : public tracktion::Plugin::EditorComponent,
                       public juce::DragAndDropTarget,
                       public juce::FileDragAndDropTarget,
                       private juce::MidiKeyboardState::Listener,
                       private juce::Timer
{
public:
    /** With a session, new samples load the same way the Rack loads them. */
    explicit KngSamplerView (KngSamplerPlugin& owner, EditSession* sessionToUse = nullptr);
    ~KngSamplerView() override;

    bool allowWindowResizing() override                               { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

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

    void setProp (const juce::Identifier& id, const juce::var& value);
    float propFloat (const juce::Identifier& id, float fallback) const;
    void loadFile (const juce::File& file);
    void chooseFile();
    void rebuildPeaks (int bars);
    int fracToX (float f) const;
    float xToFrac (int x) const;
    void paintHeader (juce::Graphics&);
    void paintWave (juce::Graphics&);

    KngSamplerPlugin& plugin;
    tracktion::Plugin::Ptr pluginRef;
    EditSession* session = nullptr;

    KngFaceStyle style;
    KngFaceLook look;

    juce::TextButton loadButton { "Load sample" };
    KngPopButton popButton;

    juce::MidiKeyboardState keyboardState;
    KngSamplerKeys keyboard;

    std::vector<std::unique_ptr<juce::Component>> knobs, pills;

    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<float> peaks;
    juce::File peaksFile;
    const juce::AudioBuffer<float>* peaksBuffer = nullptr;

    juce::Rectangle<int> panel, header, wave;
    bool hovering = false;
    int dragHandle = 0;   // 1 = loop start, 2 = loop end
    int shownNote = -2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngSamplerView)
};
