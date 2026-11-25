#include "SliceEditorView.h"

#include <algorithm>

//==============================================================================
// Per-slice controls: pick a slice, then set Reverse, Pitch and Volume for it.

void SliceEditorView::setupFx()
{
    for (auto* c : std::initializer_list<juce::Component*> { &prevSliceButton, &nextSliceButton, &sliceLabel,
                                                             &reverseButton, &pitchLabel, &pitchSlider,
                                                             &volLabel, &gainSlider })
        addAndMakeVisible (c);

    sliceLabel.setJustificationType (juce::Justification::centred);
    sliceLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    pitchLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    volLabel.setFont (juce::Font (juce::FontOptions (13.0f)));

    pitchSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    pitchSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    pitchSlider.setRange (-12.0, 12.0, 1.0);
    pitchSlider.setTextValueSuffix (" st");
    pitchSlider.setDoubleClickReturnValue (true, 0.0);

    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    gainSlider.setRange (-24.0, 6.0, 0.5);
    gainSlider.setTextValueSuffix (" dB");
    gainSlider.setDoubleClickReturnValue (true, 0.0);

    prevSliceButton.setTooltip ("Previous slice");
    nextSliceButton.setTooltip ("Next slice");
    sliceLabel.setTooltip ("The slice the controls change. Click a slice number on the waveform to pick one.");
    reverseButton.setTooltip ("Play this slice backwards.");
    pitchSlider.setTooltip ("Shift this slice up or down in semitones. Like a record speeding up, so the slice gets shorter or longer. Double-click to reset.");
    gainSlider.setTooltip ("Make this slice louder or quieter. Double-click to reset.");

    for (auto* c : std::initializer_list<juce::Component*> { &prevSliceButton, &nextSliceButton, &reverseButton,
                                                             &pitchSlider, &gainSlider })
        c->setWantsKeyboardFocus (false);

    prevSliceButton.onClick = [this] { selectSlice (selected - 1); };
    nextSliceButton.onClick = [this] { selectSlice (selected + 1); };

    auto changed = [this]
    {
        if (selected < 0 || selected >= (int) sliceFx.size())
            return;

        auto& fx = sliceFx[(size_t) selected];
        fx.reverse = reverseButton.getToggleState();
        fx.semitones = (float) pitchSlider.getValue();
        fx.gainDb = (float) gainSlider.getValue();
        repaint();
    };

    reverseButton.onClick = changed;
    pitchSlider.onValueChange = changed;
    gainSlider.onValueChange = changed;

    selectSlice (selected);
}

void SliceEditorView::layoutFxRow (juce::Rectangle<int> row)
{
    prevSliceButton.setBounds (row.removeFromLeft (28));
    row.removeFromLeft (4);
    sliceLabel.setBounds (row.removeFromLeft (70));
    row.removeFromLeft (4);
    nextSliceButton.setBounds (row.removeFromLeft (28));
    row.removeFromLeft (14);
    reverseButton.setBounds (row.removeFromLeft (90));
    row.removeFromLeft (10);
    pitchLabel.setBounds (row.removeFromLeft (44));
    pitchSlider.setBounds (row.removeFromLeft (190));
    row.removeFromLeft (14);
    volLabel.setBounds (row.removeFromLeft (56));
    gainSlider.setBounds (row.removeFromLeft (190));
}

//==============================================================================
void SliceEditorView::selectSlice (int index)
{
    const int count = (int) sliceFx.size();

    if (count == 0)
        return;

    selected = juce::jlimit (0, count - 1, index);
    const auto& fx = sliceFx[(size_t) selected];

    sliceLabel.setText ("Slice " + juce::String (selected + 1), juce::dontSendNotification);
    reverseButton.setToggleState (fx.reverse, juce::dontSendNotification);
    pitchSlider.setValue ((double) fx.semitones, juce::dontSendNotification);
    gainSlider.setValue ((double) fx.gainDb, juce::dontSendNotification);
    repaint();
}

bool SliceEditorView::selectSliceAt (juce::Point<int> p)
{
    const auto wave = getWaveArea();

    // The number badge sits at the top left of each slice
    if (p.y < wave.getY() + 6 || p.y > wave.getY() + 44)
        return false;

    std::vector<double> edges { 0.0 };

    for (auto c : cuts)
        edges.push_back (c);

    for (int i = (int) edges.size() - 1; i >= 0; --i)
    {
        const float x1 = timeToX (edges[(size_t) i]);

        if ((float) p.x >= x1 && (float) p.x <= x1 + 64.0f)
        {
            selectSlice (i);
            return true;
        }
    }

    return false;
}

// Keeps one SliceFx per slice when markers are added or removed
void SliceEditorView::syncSliceFx()
{
    if (sliceFx.size() != prevCuts.size() + 1)
        sliceFx.assign (prevCuts.size() + 1, SliceFx());

    if (cuts.size() == prevCuts.size() + 1)
    {
        // A marker was added, so one slice became two with the same settings
        size_t i = 0;

        while (i < prevCuts.size() && cuts[i] == prevCuts[i])
            ++i;

        const SliceFx copy = sliceFx[i];
        sliceFx.insert (sliceFx.begin() + (std::ptrdiff_t) i, copy);
    }
    else if (cuts.size() + 1 == prevCuts.size())
    {
        // A marker was removed, so two slices became one and the first one's settings stay
        size_t i = 0;

        while (i < cuts.size() && cuts[i] == prevCuts[i])
            ++i;

        sliceFx.erase (sliceFx.begin() + (std::ptrdiff_t) i + 1);
    }
    else if (cuts.size() != prevCuts.size())
    {
        sliceFx.assign (cuts.size() + 1, SliceFx());
    }

    prevCuts = cuts;
    selectSlice (selected);
}

//==============================================================================
void SliceEditorView::drawSliceExtras (juce::Graphics& g, juce::Rectangle<int> wave) const
{
    if (sliceFx.size() != cuts.size() + 1)
        return;

    std::vector<double> edges { 0.0 };

    for (auto c : cuts)
        edges.push_back (c);

    edges.push_back (length);

    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    for (size_t i = 0; i + 1 < edges.size(); ++i)
    {
        const float x1 = timeToX (edges[i]);
        const float x2 = timeToX (edges[i + 1]);

        if ((int) i == selected)
        {
            g.setColour (juce::Colour (0xffffd24a).withAlpha (0.14f));
            g.fillRect (x1, (float) wave.getY(), x2 - x1, (float) wave.getHeight());
            g.setColour (juce::Colour (0xffffd24a).withAlpha (0.8f));
            g.drawRect (x1, (float) wave.getY(), x2 - x1, (float) wave.getHeight(), 1.5f);
        }

        const auto& fx = sliceFx[i];
        juce::StringArray tags;

        if (fx.reverse)
            tags.add ("Rev");

        if (fx.semitones != 0.0f)
            tags.add ((fx.semitones > 0.0f ? "+" : "") + juce::String ((int) fx.semitones) + " st");

        if (fx.gainDb != 0.0f)
            tags.add ((fx.gainDb > 0.0f ? "+" : "") + juce::String (fx.gainDb, 1) + " dB");

        if (tags.size() > 0)
        {
            g.setColour (juce::Colour (0xffffd24a));
            g.drawText (tags.joinIntoString (" "),
                        juce::Rectangle<float> (x1 + 6.0f, (float) wave.getY() + 28.0f,
                                                juce::jmax (0.0f, x2 - x1 - 8.0f), 14.0f),
                        juce::Justification::centredLeft, true);
        }
    }
}
