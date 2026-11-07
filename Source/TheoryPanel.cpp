#include "TheoryPanel.h"
#include "TheoryHelper.h"

TheoryPanel::TheoryPanel()
{
    addAndMakeVisible (text);
    text.setMultiLine (true, true);
    text.setReadOnly (true);
    text.setCaretVisible (false);
    text.setScrollbarsShown (true);
    text.setFont (juce::Font (juce::FontOptions (13.0f)));
    text.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    text.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    text.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    text.setColour (juce::TextEditor::textColourId, juce::Colour (0xffd8d8d8));
}

void TheoryPanel::update (int keyRoot, int scaleIndex, int chordRoot, int chordType, int hoverNote)
{
    juce::String s;

    const Theory::Scale* scale = scaleIndex >= 0 ? &Theory::scales()[(size_t) scaleIndex] : nullptr;
    const Theory::ChordType* chord = chordType >= 0 ? &Theory::chordTypes()[(size_t) chordType] : nullptr;

    if (scale == nullptr && chord == nullptr)
    {
        s << "THEORY\n\n"
          << "Pick a Key and Scale up top to light up the notes that fit, and dim the ones that don't.\n\n"
          << "Pick a Chord to see its notes across every octave.\n\n"
          << "Turn on Stamp Chords to place a whole chord with one click.\n\n";
    }

    // Key and scale
    if (scale != nullptr)
    {
        s << "KEY: " << Theory::pitchName (keyRoot) << " " << scale->name << "\n";

        juce::StringArray notes;

        for (int step : scale->steps)
            notes.add (Theory::pitchName (keyRoot + step));

        s << "Notes: " << notes.joinIntoString ("  ") << "\n"
          << scale->about << "\n\n";

        const auto chords = Theory::diatonicChords (keyRoot, *scale);

        if (! chords.empty())
        {
            s << "Chords in this key:\n";

            for (auto& c : chords)
                s << "  " << c.numeral << "   " << Theory::chordName (c.root, Theory::chordTypes()[(size_t) c.type]) << "\n";

            s << "\nUpper case = major, lower case = minor, the little circle = diminished.\n";

            if (scale->name.contains ("Minor"))
                s << "Try i - VI - III - VII. It's one of the most used progressions in hip hop and pop.\n\n";
            else if (scale->name == "Major")
                s << "Try I - V - vi - IV. Thousands of hit songs use those four chords.\n\n";
            else
                s << "Start on the first chord and move to the 4th or 5th one to feel the key.\n\n";
        }
        else
        {
            s << "Pentatonic and blues scales are for melodies. For chords, switch to Major or Natural Minor.\n\n";
        }
    }

    // Chord
    if (chord != nullptr)
    {
        s << "CHORD: " << Theory::chordName (chordRoot, *chord) << "  (" << Theory::pitchName (chordRoot) << " " << chord->name << ")\n";

        for (int interval : chord->intervals)
            s << "  " << Theory::pitchName (chordRoot + interval) << "   " << Theory::toneLong (interval) << "\n";

        s << chord->about << "\n";

        if (scale != nullptr)
        {
            bool found = false;

            for (auto& c : Theory::diatonicChords (keyRoot, *scale))
            {
                if (c.root == Theory::wrap12 (chordRoot) && c.type == chordType)
                {
                    s << "In " << Theory::pitchName (keyRoot) << " " << scale->name << " this is the " << c.numeral << " chord.\n";
                    found = true;
                }
            }

            if (! found)
            {
                juce::StringArray outside;

                for (int interval : chord->intervals)
                    if (! Theory::inScale (chordRoot + interval, keyRoot, *scale))
                        outside.add (Theory::pitchName (chordRoot + interval));

                if (outside.isEmpty())
                    s << "Every note of this chord is in the key.\n";
                else
                    s << "Heads up: " << outside.joinIntoString (", ") << " is outside the key. That adds color, or clashes, depending on how you use it.\n";
            }
        }

        s << "\n";
    }

    // Note under the mouse
    if (hoverNote >= 0)
    {
        s << "NOTE: " << Theory::noteName (hoverNote) << "\n";

        if (scale != nullptr)
        {
            const int pc = Theory::pitchClassFrom (hoverNote, keyRoot);
            s << Theory::pitchName (hoverNote) << " is the " << Theory::intervalNames()[pc]
              << " of " << Theory::pitchName (keyRoot) << ".\n"
              << (Theory::inScale (hoverNote, keyRoot, *scale) ? "It's in the key." : "It's outside the key.") << "\n";
        }

        if (chord != nullptr)
        {
            const int tone = Theory::chordToneIndex (hoverNote, chordRoot, *chord);

            if (tone >= 0)
                s << "It's the " << Theory::toneLong (chord->intervals[(size_t) tone]) << " of "
                  << Theory::chordName (chordRoot, *chord) << ".\n";
            else
                s << "It's not in " << Theory::chordName (chordRoot, *chord) << ".\n";
        }
    }

    if (s != lastText)
    {
        lastText = s;
        text.setText (s, false);
    }
}

void TheoryPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff24272b));
    g.fillRect (getLocalBounds());
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawVerticalLine (0, 0.0f, (float) getHeight());
}

void TheoryPanel::resized()
{
    text.setBounds (getLocalBounds().reduced (8));
}
