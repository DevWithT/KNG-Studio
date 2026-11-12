#include "MidiMapDialog.h"

MidiMapDialog::MidiMapDialog (EditSession& sessionToUse)
    : session (sessionToUse)
{
    addAndMakeVisible (hint);
    hint.setText ("Turn on MIDI Learn, move a knob in KNG Studio, then twist a knob on your controller.",
                  juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));

    addAndMakeVisible (list);
    list.setModel (this);
    list.setRowHeight (26);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1b1d20));

    addAndMakeVisible (learnButton);
    learnButton.setClickingTogglesState (false);
    learnButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffc0392b));
    learnButton.onClick = [this] { session.setMidiLearn (! session.isMidiLearn()); };

    addAndMakeVisible (removeButton);
    removeButton.onClick = [this]
    {
        const int row = list.getSelectedRow();
        auto& mappings = session.getEdit().getParameterControlMappings();

        if (juce::isPositiveAndBelow (row, mappings.getNumControllerIDs()))
        {
            mappings.removeMapping (row);
            mappings.saveToEdit();
            session.save();
            list.updateContent();
            list.repaint();
        }
    };

    startTimerHz (5);
    setSize (520, 380);
}

MidiMapDialog::~MidiMapDialog()
{
    stopTimer();
    list.setModel (nullptr);
}

int MidiMapDialog::getNumRows()
{
    return session.getEdit().getParameterControlMappings().getNumControllerIDs();
}

void MidiMapDialog::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    auto& mappings = session.getEdit().getParameterControlMappings();

    if (! juce::isPositiveAndBelow (row, mappings.getNumControllerIDs()))
        return;

    if (selected)
    {
        g.setColour (juce::Colour (0xff3a8ee6).withAlpha (0.25f));
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, 1.0f, width - 4.0f, height - 2.0f), 4.0f);
    }

    const auto text = mappings.getTextForRow (row);
    auto r = juce::Rectangle<int> (8, 0, width - 16, height);

    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.setColour (juce::Colour (0xffe6893a));
    g.drawText (text.first, r.removeFromLeft (r.getWidth() / 2), juce::Justification::centredLeft, true);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawText (text.second, r, juce::Justification::centredLeft, true);
}

void MidiMapDialog::timerCallback()
{
    learnButton.setToggleState (session.isMidiLearn(), juce::dontSendNotification);
    learnButton.setButtonText (session.isMidiLearn() ? "Learning... (click to stop)" : "MIDI Learn");

    const int count = getNumRows();

    if (count != lastCount)
    {
        lastCount = count;
        list.updateContent();
        list.repaint();
    }
}

void MidiMapDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2b2f33));
}

void MidiMapDialog::resized()
{
    auto r = getLocalBounds().reduced (12);

    hint.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);

    auto buttons = r.removeFromBottom (28);
    learnButton.setBounds (buttons.removeFromLeft (200));
    removeButton.setBounds (buttons.removeFromRight (140));

    r.removeFromBottom (8);
    list.setBounds (r);
}
