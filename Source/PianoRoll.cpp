#include "PianoRoll.h"
#include "TheoryHelper.h"
#include "KngSamplerPlugin.h"
#include "KngSynthPlugin.h"

namespace te = tracktion;

//==============================================================================
PianoRoll::PianoRoll (EditSession& sessionToUse, te::MidiClip& clipToEdit)
    : session (sessionToUse), clip (&clipToEdit)
{
    setWantsKeyboardFocus (true);

    for (auto* c : std::initializer_list<juce::Component*> { &gridBox, &quantiseButton, &hintLabel,
                                                             &keyLabel, &keyBox, &scaleBox, &chordLabel,
                                                             &chordRootBox, &chordTypeBox, &stampButton, &theory })
        addAndMakeVisible (c);

    // Grid
    gridBox.addItem ("Grid 1/4", 1);
    gridBox.addItem ("Grid 1/8", 2);
    gridBox.addItem ("Grid 1/16", 3);
    gridBox.addItem ("Grid 1/32", 4);
    gridBox.addItem ("Grid Off", 5);
    gridBox.setSelectedId (3, juce::dontSendNotification);
    gridBox.onChange = [this] { repaint(); };

    quantiseButton.onClick = [this] { quantise(); };

    addAndMakeVisible (theoryButton);
    theoryButton.setClickingTogglesState (false);
    theoryButton.onClick = [this] { setTheoryOpen (! theoryOpen); };

    addAndMakeVisible (slideButton);
    slideButton.setTooltip ("Slide (G): the note before glides into the selected note. Again to remove.");
    slideButton.onClick = [this] { toggleSlideOnSelected(); };

    hintLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    hintLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    hintLabel.setText ("Click to add, drag to move, drag the right edge to resize. Hold right-click and drag to erase.",
                       juce::dontSendNotification);

    // Theory controls
    for (int i = 0; i < 12; ++i)
    {
        keyBox.addItem (Theory::pitchName (i), i + 1);
        chordRootBox.addItem (Theory::pitchName (i), i + 1);
    }

    scaleBox.addItem ("No scale", 1);

    for (size_t i = 0; i < Theory::scales().size(); ++i)
        scaleBox.addItem (Theory::scales()[i].name, (int) i + 2);

    chordTypeBox.addItem ("No chord", 1);

    for (size_t i = 0; i < Theory::chordTypes().size(); ++i)
        chordTypeBox.addItem (Theory::chordTypes()[i].name, (int) i + 2);

    // Restore what this clip used last time
    keyBox.setSelectedId       ((int) clip->state.getProperty ("kngKey", 0) + 1, juce::dontSendNotification);
    scaleBox.setSelectedId     ((int) clip->state.getProperty ("kngScale", -1) + 2, juce::dontSendNotification);
    chordRootBox.setSelectedId ((int) clip->state.getProperty ("kngChordRoot", 0) + 1, juce::dontSendNotification);
    chordTypeBox.setSelectedId ((int) clip->state.getProperty ("kngChordType", -1) + 2, juce::dontSendNotification);
    stampButton.setToggleState ((bool) clip->state.getProperty ("kngStamp", false), juce::dontSendNotification);

    for (auto* box : { &keyBox, &scaleBox, &chordRootBox, &chordTypeBox })
        box->onChange = [this] { theoryChanged(); };

    stampButton.onClick = [this] { clip->state.setProperty ("kngStamp", stampButton.getToggleState(), nullptr); };

    // Tooltips
    gridBox.setTooltip ("Note grid size for snapping (hold Alt while dragging to ignore it)");
    quantiseButton.setTooltip ("Snap notes to the grid (Q). Works on selected notes, or all if none are selected.");
    theoryButton.setTooltip ("Show scales, chords and music theory help");
    keyBox.setTooltip ("The song's key");
    scaleBox.setTooltip ("Highlights the notes that fit the key; notes outside it turn red");
    chordRootBox.setTooltip ("Root note of the chord to highlight");
    chordTypeBox.setTooltip ("Chord to highlight across every octave");
    stampButton.setTooltip ("Click once in the grid to place the whole chord");

    viewBeat = getClipStartBeat();

    // Centre on the notes if there are any
    auto& seq = clip->getSequence();

    if (seq.getNumNotes() > 0)
        topNote = juce::jlimit (12, 127, seq.getNoteNumberRange().getEnd() + 6);

    rebuildChordButtons();
    updateTheoryPanel();

    setTheoryOpen ((bool) clip->state.getProperty ("kngTheoryOpen", false));
    startTimerHz (30);
    setSize (1240, 700);
}

PianoRoll::~PianoRoll()
{
    stopTimer();
}

//==============================================================================
// Theory

int PianoRoll::getKeyRoot() const      { return juce::jmax (0, keyBox.getSelectedId() - 1); }
int PianoRoll::getScaleIndex() const   { return scaleBox.getSelectedId() - 2; }
int PianoRoll::getChordRoot() const    { return juce::jmax (0, chordRootBox.getSelectedId() - 1); }
int PianoRoll::getChordType() const    { return chordTypeBox.getSelectedId() - 2; }

void PianoRoll::theoryChanged()
{
    clip->state.setProperty ("kngKey", getKeyRoot(), nullptr);
    clip->state.setProperty ("kngScale", getScaleIndex(), nullptr);
    clip->state.setProperty ("kngChordRoot", getChordRoot(), nullptr);
    clip->state.setProperty ("kngChordType", getChordType(), nullptr);

    // Only rebuild the chord buttons when the key changes
    if (getKeyRoot() != builtKey || getScaleIndex() != builtScale)
        rebuildChordButtons();

    for (int i = 0; i < chordButtons.size(); ++i)
        chordButtons[i]->setToggleState ((bool) chordButtons[i]->getProperties()["root"].equals (getChordRoot())
                                         && (bool) chordButtons[i]->getProperties()["type"].equals (getChordType()),
                                         juce::dontSendNotification);

    updateTheoryPanel();
    repaint();
}

void PianoRoll::rebuildChordButtons()
{
    chordButtons.clear();
    builtKey = getKeyRoot();
    builtScale = getScaleIndex();

    if (builtScale >= 0)
    {
        for (auto& dc : Theory::diatonicChords (builtKey, Theory::scales()[(size_t) builtScale]))
        {
            auto* b = chordButtons.add (new juce::TextButton (dc.numeral + " " + Theory::chordName (dc.root, Theory::chordTypes()[(size_t) dc.type])));
            b->getProperties().set ("root", dc.root);
            b->getProperties().set ("type", dc.type);
            b->setClickingTogglesState (false);
            b->setToggleState (dc.root == getChordRoot() && dc.type == getChordType(), juce::dontSendNotification);

            const int root = dc.root, type = dc.type;
            b->onClick = [this, root, type]
            {
                chordRootBox.setSelectedId (root + 1, juce::dontSendNotification);
                chordTypeBox.setSelectedId (type + 2, juce::dontSendNotification);
                theoryChanged();
            };

            addAndMakeVisible (b);
        }
    }

    resized();
}

void PianoRoll::updateTheoryPanel()
{
    theory.update (getKeyRoot(), getScaleIndex(), getChordRoot(), getChordType(), hoverNote);
}

//==============================================================================
// Geometry

juce::Rectangle<int> PianoRoll::getTheoryArea() const
{
    auto r = getLocalBounds().withTrimmedTop (toolbarHeight);
    return r.removeFromRight (theoryWidth);
}

juce::Rectangle<int> PianoRoll::getKeyboardArea() const
{
    auto r = getLocalBounds().withTrimmedTop (toolbarHeight + rulerHeight)
                             .withTrimmedBottom (velocityHeight)
                             .withTrimmedRight (theoryWidth);
    return r.removeFromLeft (keyboardWidth);
}

juce::Rectangle<int> PianoRoll::getGridArea() const
{
    return getLocalBounds().withTrimmedTop (toolbarHeight + rulerHeight)
                           .withTrimmedBottom (velocityHeight)
                           .withTrimmedLeft (keyboardWidth)
                           .withTrimmedRight (theoryWidth);
}

juce::Rectangle<int> PianoRoll::getRulerArea() const
{
    return { keyboardWidth, toolbarHeight, juce::jmax (0, getWidth() - keyboardWidth - theoryWidth), rulerHeight };
}

juce::Rectangle<int> PianoRoll::getVelocityArea() const
{
    auto r = getLocalBounds().withTrimmedTop (toolbarHeight).withTrimmedRight (theoryWidth);
    return r.removeFromBottom (velocityHeight).withTrimmedLeft (keyboardWidth);
}

double PianoRoll::xToBeat (int x) const
{
    return viewBeat + (x - getGridArea().getX()) / pxPerBeat;
}

int PianoRoll::beatToX (double beat) const
{
    return getGridArea().getX() + juce::roundToInt ((beat - viewBeat) * pxPerBeat);
}

int PianoRoll::noteToY (int note) const
{
    return getGridArea().getY() + (topNote - note) * noteHeight;
}

int PianoRoll::yToNote (int y) const
{
    const int rows = (int) std::floor ((y - getGridArea().getY()) / (double) noteHeight);
    return juce::jlimit (0, 127, topNote - rows);
}

juce::Rectangle<int> PianoRoll::getNoteRect (te::MidiNote& n) const
{
    const int x = beatToX (n.getStartBeat().inBeats());
    const int w = juce::jmax (3, juce::roundToInt (n.getLengthBeats().inBeats() * pxPerBeat));
    return { x, noteToY (n.getNoteNumber()) + 1, w, noteHeight - 2 };
}

//==============================================================================
// Grid

double PianoRoll::getGridBeats() const
{
    switch (gridBox.getSelectedId())
    {
        case 1:  return 1.0;
        case 2:  return 0.5;
        case 3:  return 0.25;
        case 4:  return 0.125;
        default: return 0.0;
    }
}

double PianoRoll::snapBeat (double beat) const
{
    const double g = getGridBeats();
    return g > 0.0 ? std::round (beat / g) * g : beat;
}

double PianoRoll::snapFloor (double beat) const
{
    const double g = getGridBeats();
    return g > 0.0 ? std::floor (beat / g) * g : beat;
}

//==============================================================================
// Clip

double PianoRoll::getClipStartBeat() const
{
    return clip->getContentBeatAtTime (clip->getPosition().getStart()).inBeats();
}

double PianoRoll::getClipEndBeat() const
{
    return clip->getContentBeatAtTime (clip->getPosition().getEnd()).inBeats();
}

void PianoRoll::ensureClipCovers (double endBeat)
{
    if (endBeat <= getClipEndBeat())
        return;

    // Grow the clip to the next whole bar
    const double newEndBeat = std::ceil (endBeat / 4.0) * 4.0;
    clip->setEnd (clip->getTimeOfContentBeat (te::BeatPosition::fromBeats (newEndBeat)), true);
}

juce::UndoManager* PianoRoll::um()
{
    return &session.getEdit().getUndoManager();
}

//==============================================================================
void PianoRoll::timerCallback()
{
    // Clip was deleted; the dock clears this panel
    if (clip->getParent() == nullptr)
        return;

    cleanSelection();
    repaint();
}

void PianoRoll::resized()
{
    auto tb = getLocalBounds().removeFromTop (toolbarHeight).reduced (6, 4);
    auto row1 = tb.removeFromTop (28);

    gridBox.setBounds (row1.removeFromLeft (110));
    row1.removeFromLeft (6);
    quantiseButton.setBounds (row1.removeFromLeft (110));
    row1.removeFromLeft (6);
    theoryButton.setBounds (row1.removeFromLeft (80));
    row1.removeFromLeft (6);
    slideButton.setBounds (row1.removeFromLeft (70));
    row1.removeFromLeft (12);
    hintLabel.setBounds (row1);

    // Theory controls only show while Theory is on
    for (auto* c : std::initializer_list<juce::Component*> { &keyLabel, &keyBox, &scaleBox, &chordLabel,
                                                             &chordRootBox, &chordTypeBox, &stampButton, &theory })
        c->setVisible (theoryOpen);

    for (auto* b : chordButtons)
        b->setVisible (theoryOpen);

    if (theoryOpen)
    {
        tb.removeFromTop (4);
        auto row2 = tb.removeFromTop (28);

        keyLabel.setBounds (row2.removeFromLeft (32));
        keyBox.setBounds (row2.removeFromLeft (60));
        row2.removeFromLeft (4);
        scaleBox.setBounds (row2.removeFromLeft (150));
        row2.removeFromLeft (14);
        chordLabel.setBounds (row2.removeFromLeft (44));
        chordRootBox.setBounds (row2.removeFromLeft (60));
        row2.removeFromLeft (4);
        chordTypeBox.setBounds (row2.removeFromLeft (120));
        row2.removeFromLeft (8);
        stampButton.setBounds (row2.removeFromLeft (120));
        row2.removeFromLeft (8);

        for (auto* b : chordButtons)
        {
            b->setBounds (row2.removeFromLeft (76));
            row2.removeFromLeft (3);
        }

        theory.setBounds (getTheoryArea());
    }
}

void PianoRoll::setTheoryOpen (bool shouldBeOpen)
{
    theoryOpen = shouldBeOpen;
    clip->state.setProperty ("kngTheoryOpen", theoryOpen, nullptr);
    theoryButton.setToggleState (theoryOpen, juce::dontSendNotification);

    toolbarHeight = theoryOpen ? 70 : 38;
    theoryWidth   = theoryOpen ? 270 : 0;

    resized();
    repaint();
}

//==============================================================================
PianoRollWindow::PianoRollWindow (EditSession& session, te::MidiClip& c)
    : DocumentWindow ("Piano Roll", juce::Colour (0xff1b1d20), DocumentWindow::allButtons),
      clip (&c)
{
    if (auto* t = c.getTrack())
        setName ("Piano Roll - " + t->getName());

    setUsingNativeTitleBar (true);
    setContentOwned (new PianoRoll (session, c), true);
    setResizable (true, false);
    centreWithSize (1240, 700);
    setVisible (true);
}
