#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** Select and Paint tools for the Playlist. */
class PlaylistToolbar : public juce::Component,
                        private juce::Timer
{
public:
    explicit PlaylistToolbar (EditSession& sessionToUse) : session (sessionToUse)
    {
        for (auto* b : { &selectButton, &paintButton })
        {
            addAndMakeVisible (b);
            b->setClickingTogglesState (false);
            b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3a8ee6));
        }

        selectButton.onClick = [this] { session.setPlaylistTool (0); };
        paintButton.onClick  = [this] { session.setPlaylistTool (1); };
        selectButton.setTooltip ("Select: drag blocks to move them (Ctrl copies). Right-click a block for more.");
        paintButton.setTooltip ("Paint: click or drag in a lane to place the current pattern. Right-drag erases.");

        addAndMakeVisible (hint);
        hint.setFont (juce::Font (juce::FontOptions (12.0f)));
        hint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));
        hint.setText ("Drag a pattern from the list into a lane, or pick Paint and click in a lane.", juce::dontSendNotification);

        startTimerHz (8);
    }

    ~PlaylistToolbar() override   { stopTimer(); }

    void resized() override
    {
        auto r = getLocalBounds();
        selectButton.setBounds (r.removeFromLeft (70));
        r.removeFromLeft (4);
        paintButton.setBounds (r.removeFromLeft (70));
        r.removeFromLeft (12);
        hint.setBounds (r);
    }

private:
    void timerCallback() override
    {
        selectButton.setToggleState (session.getPlaylistTool() == 0, juce::dontSendNotification);
        paintButton.setToggleState (session.getPlaylistTool() == 1, juce::dontSendNotification);
    }

    EditSession& session;
    juce::TextButton selectButton { "Select" }, paintButton { "Paint" };
    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaylistToolbar)
};
