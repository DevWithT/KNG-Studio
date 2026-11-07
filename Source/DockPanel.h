#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

//==============================================================================
/** Tabbed panel docked along the bottom of the main window. */
class DockPanel : public juce::Component,
                  private juce::Timer
{
public:
    enum Tab { rackTab = 0, pianoRollTab, instrumentTab, fxTab, compTab, mixerTab, numTabs };

    DockPanel();
    ~DockPanel() override;

    /** Pass nullptr to clear everything before a project closes. */
    void setSession (EditSession* sessionToUse);

    void showPianoRoll (tracktion::MidiClip& clip);
    void showInstrument (tracktion::Track& track, bool isDrumTrack);
    void showFx (tracktion::PluginList& list, const juce::String& title);
    void showComp (tracktion::WaveAudioClip& clip);
    void showTab (Tab tab);
    Tab getCurrentTab() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** Holds whatever is open in one tab, or a hint when it's empty. */
    struct Slot : public juce::Component
    {
        void setContent (std::unique_ptr<juce::Component> newContent);
        void clear();
        void paint (juce::Graphics&) override;
        void resized() override;

        std::unique_ptr<juce::Component> content;
        juce::String message;
    };

    void timerCallback() override;
    void resetTabName (Tab tab);
    static juce::String getTrackName (tracktion::Track* track);

    EditSession* session = nullptr;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    Slot* slots[numTabs] {};   // owned by the tabs

    // What each tab is showing, so it can clear itself if that gets deleted
    tracktion::MidiClip::Ptr pianoClip;
    tracktion::Track::Ptr instrumentTrack, fxTrack;
    tracktion::WaveAudioClip::Ptr compClip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DockPanel)
};

//==============================================================================
/** Thin bar above the dock that you drag to resize it. */
class DockDivider : public juce::Component
{
public:
    DockDivider()   { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); }

    std::function<void()> onDragStart;
    std::function<void (int)> onDrag;

    void mouseDown (const juce::MouseEvent&) override         { if (onDragStart) onDragStart(); }
    void mouseDrag (const juce::MouseEvent& e) override       { if (onDrag) onDrag (e.getDistanceFromDragStartY()); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillRoundedRectangle (getLocalBounds().withSizeKeepingCentre (60, 3).toFloat(), 1.5f);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DockDivider)
};
