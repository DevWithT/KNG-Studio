#include "SliceEditorView.h"
#include "SlicePitch.h"

#include <vector>

//==============================================================================
// Snap to Key: find each slice's note and set its Pitch so it lands on the nearest note of the key.

void SliceEditorView::setupKey()
{
    for (auto* c : std::initializer_list<juce::Component*> { &keyLabel, &keyBox, &scaleBox, &snapKeyButton })
    {
        addAndMakeVisible (c);
        c->setWantsKeyboardFocus (false);
    }

    keyLabel.setFont (juce::Font (juce::FontOptions (13.0f)));

    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    for (int i = 0; i < 12; ++i)
        keyBox.addItem (noteNames[i], i + 1);

    keyBox.setSelectedId (1, juce::dontSendNotification);

    const auto& scales = KngSlicePitch::scales();

    for (int i = 0; i < (int) scales.size(); ++i)
        scaleBox.addItem (scales[(size_t) i].name, i + 1);

    scaleBox.setSelectedId (2, juce::dontSendNotification);   // Minor

    keyBox.setTooltip ("The key of your song.");
    scaleBox.setTooltip ("The scale to snap to. Chromatic snaps to the nearest semitone.");
    snapKeyButton.setTooltip ("Listen to each slice, find its note, and set its Pitch so it lands on the nearest note of this key and scale. Slices with no clear note (drums, noise) are left alone.");

    snapKeyButton.onClick = [this] { snapToKey(); };

    addAndMakeVisible (recordPitchToggle);
    recordPitchToggle.setButtonText ("Record-style pitch");
    recordPitchToggle.setTooltip ("Off: pitch keeps each slice the same length (a real pitch shift). "
                                  "On: pitch also changes the speed, like a sped-up or slowed-down record.");
}

void SliceEditorView::layoutKeyRow (juce::Rectangle<int> row)
{
    keyLabel.setBounds (row.removeFromLeft (36));
    keyBox.setBounds (row.removeFromLeft (64));
    row.removeFromLeft (8);
    scaleBox.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (10);
    snapKeyButton.setBounds (row.removeFromLeft (130));
    row.removeFromLeft (14);
    recordPitchToggle.setBounds (row.removeFromLeft (170));
}

void SliceEditorView::snapToKey()
{
    const auto source = clip->getAudioFile().getFile();
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (source.existsAsFile() ? formats.createReaderFor (source) : nullptr);

    if (reader == nullptr || reader->numChannels == 0)
    {
        hintLabel.setText ("Couldn't read this clip's audio file.", juce::dontSendNotification);
        return;
    }

    const double offset = clip->getPosition().getOffset().inSeconds();
    const int root = juce::jlimit (0, 11, keyBox.getSelectedId() - 1);
    const auto& scaleList = KngSlicePitch::scales();
    const auto& scale = scaleList[(size_t) juce::jlimit (0, (int) scaleList.size() - 1, scaleBox.getSelectedId() - 1)].notes;

    std::vector<double> edges { 0.0 };

    for (auto c : cuts)
        edges.push_back (c);

    edges.push_back (length);

    int snapped = 0, skipped = 0;

    for (size_t i = 0; i + 1 < edges.size() && i < sliceFx.size(); ++i)
    {
        const double sliceLen = edges[i + 1] - edges[i];

        // Skip the first tenth (the attack) and look at up to about 0.2 seconds after it
        const double from = offset + edges[i] + sliceLen * 0.1;
        const int n = (int) juce::jmin (8192.0, sliceLen * 0.9 * reader->sampleRate);

        if (n < 512)
        {
            ++skipped;
            continue;
        }

        juce::AudioBuffer<float> buffer ((int) reader->numChannels, n);
        buffer.clear();
        reader->read (&buffer, 0, n, (juce::int64) (from * reader->sampleRate), true, true);

        std::vector<float> mono ((size_t) n, 0.0f);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int k = 0; k < n; ++k)
                mono[(size_t) k] += buffer.getSample (ch, k) / (float) buffer.getNumChannels();

        const float hz = KngSlicePitch::detect (mono.data(), n, reader->sampleRate);

        if (hz <= 0.0f)
        {
            ++skipped;
            continue;
        }

        sliceFx[i].semitones = KngSlicePitch::semitonesToKey (hz, root, scale);
        ++snapped;
    }

    selectSlice (selected);
    repaint();

    hintLabel.setText ("Snapped " + juce::String (snapped) + (snapped == 1 ? " slice" : " slices")
                         + (skipped > 0 ? ". " + juce::String (skipped) + " had no clear note and were left alone." : "."),
                       juce::dontSendNotification);
}
