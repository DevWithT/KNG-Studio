#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** Select, Paint and Cut tools for the Playlist. Also keeps the Playlist tidy as you work. */
class PlaylistToolbar : public juce::Component,
                        private juce::Timer
{
public:
    explicit PlaylistToolbar (EditSession& sessionToUse) : session (sessionToUse)
    {
        for (auto* b : { &selectButton, &paintButton, &cutButton })
        {
            addAndMakeVisible (b);
            b->setClickingTogglesState (false);
            b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3a8ee6));
        }

        selectButton.onClick = [this] { session.setPlaylistTool (0); };
        paintButton.onClick  = [this] { session.setPlaylistTool (1); };
        cutButton.onClick    = [this] { session.setPlaylistTool (2); };

        selectButton.setTooltip ("Select: drag blocks to move them (Ctrl copies). Drag a right edge to stretch. Right-click for more.");
        paintButton.setTooltip ("Paint: click or drag in a lane to place the current pattern. Right-drag erases.");
        cutButton.setTooltip ("Cut: click a block to slice it in two");

        addAndMakeVisible (hint);
        hint.setFont (juce::Font (juce::FontOptions (12.0f)));
        hint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));
        hint.setText ("Drag a pattern from the list into a lane. Stretch a block by its right edge to repeat it.", juce::dontSendNotification);

        startTimerHz (8);
    }

    ~PlaylistToolbar() override   { stopTimer(); }

    void resized() override
    {
        auto r = getLocalBounds();
        selectButton.setBounds (r.removeFromLeft (70));
        r.removeFromLeft (4);
        paintButton.setBounds (r.removeFromLeft (70));
        r.removeFromLeft (4);
        cutButton.setBounds (r.removeFromLeft (60));
        r.removeFromLeft (12);
        hint.setBounds (r);
    }

private:
    void timerCallback() override
    {
        const int tool = session.getPlaylistTool();
        selectButton.setToggleState (tool == 0, juce::dontSendNotification);
        paintButton.setToggleState (tool == 1, juce::dontSendNotification);
        cutButton.setToggleState (tool == 2, juce::dontSendNotification);

        session.tidyPlaylist();
    }

    EditSession& session;
    juce::TextButton selectButton { "Select" }, paintButton { "Paint" }, cutButton { "Cut" };
    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaylistToolbar)
};
