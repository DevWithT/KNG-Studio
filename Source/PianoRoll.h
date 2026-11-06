#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

class PianoRoll : public juce::Component,
                  private juce::Timer
{
public:
    PianoRoll (EditSession& sessionToUse, tracktion::MidiClip& clipToEdit);
    ~PianoRoll() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    enum class DragMode { none, move, resize, velocity };

    struct Orig
    {
        tracktion::MidiNote* note;
        double start, length;
        int pitch;
    };

    void timerCallback() override;

    // Geometry
    juce::Rectangle<int> getKeyboardArea() const;
    juce::Rectangle<int> getGridArea() const;
    juce::Rectangle<int> getVelocityArea() const;
    juce::Rectangle<int> getNoteRect (tracktion::MidiNote&) const;
    double xToBeat (int x) const;
    int beatToX (double beat) const;
    int noteToY (int note) const;
    int yToNote (int y) const;

    // Grid
    double getGridBeats() const;
    double snapBeat (double beat) const;
    double snapFloor (double beat) const;

    // Clip
    double getClipStartBeat() const;
    double getClipEndBeat() const;
    void ensureClipCovers (double endBeat);
    juce::UndoManager* um();

    // Notes
    tracktion::MidiNote* findNoteAt (juce::Point<int> p, bool& onRightEdge);
    bool isValid (tracktion::MidiNote*) const;
    void cleanSelection();
    void applyVelocity (juce::Point<int> p);
    void previewNote (int note, int velocity);
    void quantise();
    void deleteSelected();

    EditSession& session;
    tracktion::MidiClip::Ptr clip;

    juce::ComboBox gridBox;
    juce::TextButton quantiseButton { "Quantize (Q)" };
    juce::Label hintLabel;

    juce::Array<tracktion::MidiNote*> selected;
    std::vector<Orig> origs;

    DragMode dragMode = DragMode::none;
    double dragStartBeat = 0.0;
    int dragStartNote = 60;
    int lastPreviewNote = -1;
    double lastLength = 0.25;

    double viewBeat = 0.0;
    double pxPerBeat = 80.0;
    int topNote = 84;

    static constexpr int toolbarHeight  = 34;
    static constexpr int keyboardWidth  = 60;
    static constexpr int noteHeight     = 14;
    static constexpr int velocityHeight = 70;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};

//==============================================================================
class PianoRollWindow : public juce::DocumentWindow
{
public:
    PianoRollWindow (EditSession& session, tracktion::MidiClip& clip);

    void closeButtonPressed() override          { setVisible (false); }
    tracktion::MidiClip* getClip() const        { return clip.get(); }

private:
    tracktion::MidiClip::Ptr clip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollWindow)
};
