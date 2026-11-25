#include "SliceEditorView.h"

#include <cmath>

//==============================================================================
// Tempo match: tell the editor what tempo the clip was played at, and Chop to Rack
// places the Chops notes on the song's tempo.

void SliceEditorView::setupTempo()
{
    for (auto* c : std::initializer_list<juce::Component*> { &matchToggle, &sourceBpm, &detectBpmButton })
    {
        addAndMakeVisible (c);
        c->setWantsKeyboardFocus (false);
    }

    sourceBpm.setSliderStyle (juce::Slider::IncDecButtons);
    sourceBpm.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 22);
    sourceBpm.setRange (40.0, 300.0, 0.1);
    sourceBpm.setValue (guessSourceBpm(), juce::dontSendNotification);
    sourceBpm.setEnabled (false);
    detectBpmButton.setEnabled (false);

    matchToggle.setTooltip ("Chop to Rack places the chops on the song's tempo. The slices still play at their original speed.");
    sourceBpm.setTooltip ("The tempo this clip was played at.");
    detectBpmButton.setTooltip ("Guess the tempo from the clip length, assuming the clip is a whole number of bars in 4/4.");

    matchToggle.onClick = [this]
    {
        const bool on = matchToggle.getToggleState();
        sourceBpm.setEnabled (on);
        detectBpmButton.setEnabled (on);
    };

    detectBpmButton.onClick = [this] { sourceBpm.setValue (guessSourceBpm(), juce::dontSendNotification); };
}

double SliceEditorView::guessSourceBpm() const
{
    const double projectBpm = session.getBpmAt (clipStart);
    double best = projectBpm;
    double bestDiff = 1.0e9;

    // 1, 2, 4, 8 or 16 bars of 4/4. Of the tempos that make sense, take the one closest to the song's.
    for (int bars : { 1, 2, 4, 8, 16 })
    {
        const double bpm = 240.0 * (double) bars / length;

        if (bpm < 70.0 || bpm > 190.0)
            continue;

        const double diff = std::abs (bpm - projectBpm);

        if (diff < bestDiff)
        {
            bestDiff = diff;
            best = bpm;
        }
    }

    return juce::jlimit (40.0, 300.0, best);
}
