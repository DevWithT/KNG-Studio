#include "SliceEditorView.h"

#include <algorithm>
#include <cmath>

namespace te = tracktion;

//==============================================================================
// Tap-in cuts: play the clip and press T (or Cut Here) on every hit.

void SliceEditorView::setupTap()
{
    addAndMakeVisible (playButton);
    addAndMakeVisible (tapButton);
    addAndMakeVisible (tapLabel);

    playButton.setTooltip ("Play the clip from its start. Space also starts and stops it.");
    tapButton.setTooltip ("Add a cut at the playhead. Press T on your keyboard for the same thing.");

    tapLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    tapLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    tapLabel.setText ("Tap-in: press Play, then press T on every hit.", juce::dontSendNotification);

    playButton.onClick = [this] { if (playing) stopPlay(); else startPlay(); };
    tapButton.onClick  = [this] { tapCut(); };

    // The window itself keeps the keyboard, so T and Space always reach keyPressed
    playButton.setWantsKeyboardFocus (false);
    tapButton.setWantsKeyboardFocus (false);
    setWantsKeyboardFocus (true);
}

SliceEditorView::~SliceEditorView()
{
    stopTimer();

    if (playing)
        stopPlay();
}

void SliceEditorView::layoutTapRow (juce::Rectangle<int> row)
{
    playButton.setBounds (row.removeFromLeft (100));
    row.removeFromLeft (6);
    tapButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (12);
    auto tempo = row.removeFromRight (300);
    tapLabel.setBounds (row);
    matchToggle.setBounds (tempo.removeFromLeft (112));
    tempo.removeFromLeft (6);
    detectBpmButton.setBounds (tempo.removeFromRight (64));
    tempo.removeFromRight (6);
    sourceBpm.setBounds (tempo);
}

//==============================================================================
double SliceEditorView::playheadTime() const
{
    return juce::jlimit (0.0, length, session.getTransport().getPosition().inSeconds() - clipStart);
}

void SliceEditorView::drawPlayhead (juce::Graphics& g, juce::Rectangle<int> wave) const
{
    if (! playing)
        return;

    const float x = timeToX (playheadTime());
    g.setColour (juce::Colour (0xffff5a4a));
    g.drawLine (x, (float) wave.getY(), x, (float) wave.getBottom(), 2.0f);
}

//==============================================================================
void SliceEditorView::startPlay()
{
    auto& transport = session.getTransport();

    if (transport.isRecording())
        return;

    savedPosition = transport.getPosition().inSeconds();

    if (transport.isPlaying())
        transport.stop (false, false);

    transport.setPosition (te::TimePosition::fromSeconds (clipStart));
    session.play();

    playing = true;
    playButton.setButtonText ("Stop");
    tapLabel.setText ("Tap-in: press T on every hit.", juce::dontSendNotification);
    startTimerHz (30);
    grabKeyboardFocus();
    repaint();
}

void SliceEditorView::stopPlay()
{
    stopTimer();

    if (! playing)
        return;

    playing = false;

    auto& transport = session.getTransport();

    if (transport.isPlaying() && ! transport.isRecording())
        transport.stop (false, false);

    transport.setPosition (te::TimePosition::fromSeconds (savedPosition));
    playButton.setButtonText ("Play");
    repaint();
}

void SliceEditorView::tapCut()
{
    if (! playing)
        return;

    if (cuts.size() >= 15)
    {
        tapLabel.setText ("15 cuts is the most. Drag a marker or press Clear.", juce::dontSendNotification);
        return;
    }

    const double t = playheadTime();

    if (t < 0.01 || t > length - 0.01)
        return;

    for (auto c : cuts)
        if (std::abs (c - t) < 0.03)
            return;

    cuts.push_back (t);
    std::sort (cuts.begin(), cuts.end());
    updateInfo();
    repaint();
}

void SliceEditorView::timerCallback()
{
    const double t = session.getTransport().getPosition().inSeconds() - clipStart;

    // Stop at the end of the clip, or if something else stopped or moved the transport
    if (! session.getTransport().isPlaying() || t < -0.05 || t > length + 0.02)
    {
        stopPlay();
        return;
    }

    repaint (getWaveArea());
}

//==============================================================================
bool SliceEditorView::keyPressed (const juce::KeyPress& key)
{
    const auto ch = key.getTextCharacter();

    if (ch == 't' || ch == 'T')
    {
        // Holding the key down should give one cut, not a stream of them
        if (! tapKeyHeld)
        {
            tapKeyHeld = true;
            tapCut();
        }

        return true;
    }

    if (key == juce::KeyPress::spaceKey)
    {
        if (playing)
            stopPlay();
        else
            startPlay();

        return true;
    }

    return false;
}

bool SliceEditorView::keyStateChanged (bool)
{
    if (! juce::KeyPress::isKeyCurrentlyDown ('t') && ! juce::KeyPress::isKeyCurrentlyDown ('T'))
        tapKeyHeld = false;

    return false;
}
