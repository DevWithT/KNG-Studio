#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "TheoryPanel.h"

class PianoRoll : public juce::Component,
                  private juce::Timer
{
public:
    PianoRoll (EditSession& sessionToUse, tracktion::MidiClip& clipToEdit);
    ~PianoRoll() override;

    void paint (juce::Graphics&) override;   // in PianoRollPaint.cpp
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
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
    juce::Rectangle<int> getTheoryArea() const;
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
    void previewNotes (const juce::Array<int>& notes, int velocity);
    void quantise();
    void deleteSelected();
    void eraseAt (juce::Point<int> p);

    // Theory
    int getKeyRoot() const;
    int getScaleIndex() const;
    int getChordRoot() const;
    int getChordType() const;
    void theoryChanged();
    void rebuildChordButtons();
    void updateTheoryPanel();

    // Painting (PianoRollPaint.cpp)
    void paintRows (juce::Graphics&, juce::Rectangle<int> grid);
    void paintGridLines (juce::Graphics&, juce::Rectangle<int> grid);
    void paintNotes (juce::Graphics&, juce::Rectangle<int> grid);
    void paintKeyboard (juce::Graphics&);
    void paintVelocity (juce::Graphics&);

    EditSession& session;
    tracktion::MidiClip::Ptr clip;

    // Row 1
    juce::ComboBox gridBox;
    juce::TextButton quantiseButton { "Quantize (Q)" };
    juce::Label hintLabel;

    // Row 2: theory
    juce::Label keyLabel { {}, "Key" }, chordLabel { {}, "Chord" };
    juce::ComboBox keyBox, scaleBox, chordRootBox, chordTypeBox;
    juce::ToggleButton stampButton { "Stamp chords" };
    juce::OwnedArray<juce::TextButton> chordButtons;
    int builtKey = -1, builtScale = -2;

    TheoryPanel theory;

    juce::Array<tracktion::MidiNote*> selected;
    std::vector<Orig> origs;

    DragMode dragMode = DragMode::none;
    bool erasing = false;
    double dragStartBeat = 0.0;
    int dragStartNote = 60;
    int lastPreviewNote = -1;
    double lastLength = 0.25;
    int hoverNote = -1;

    double viewBeat = 0.0;
    double pxPerBeat = 80.0;
    int topNote = 84;

    static constexpr int toolbarHeight  = 70;
    static constexpr int theoryWidth    = 270;
    static constexpr int keyboardWidth  = 70;
    static constexpr int noteHeight     = 16;
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
