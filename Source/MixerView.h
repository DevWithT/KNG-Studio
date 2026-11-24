#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "LevelMeter.h"

//==============================================================================
/** One vertical channel strip. trackIndex -1 means the master. */
class MixerStrip : public juce::Component,
                   private juce::Timer
{
public:
    MixerStrip (EditSession& sessionToUse, int trackIndexToUse);
    ~MixerStrip() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    bool isMaster() const   { return trackIndex < 0; }

    EditSession& session;
    const int trackIndex;
    bool isBus = false;

    juce::Label nameLabel;
    juce::TextButton fxButton { "FX" }, muteButton { "M" }, soloButton { "S" };
    juce::Label sendLabel { {}, "Reverb" }, panLabel { {}, "Pan" }, widthLabel { {}, "Width" };
    juce::Slider panKnob, widthKnob, fader;
    LevelMeter meter;

    struct InsertButton : public juce::TextButton
    {
        std::function<void()> onRightClick;
        std::function<void (int)> onReorder;   // slots moved on release: negative is up
        int slotsUp = 0, slotsDown = 0;        // how far this button can travel
        bool dragged = false;                  // true after a drag, so the release is not a click
        int restTop = 0;
        static constexpr int pitch = 20;       // a button is 18 px tall with 2 px between

        void mouseDrag (const juce::MouseEvent& e) override
        {
            juce::TextButton::mouseDrag (e);

            if (e.mods.isPopupMenu())
                return;

            const int dy = e.getScreenPosition().y - e.getMouseDownScreenPosition().y;

            if (! dragged && std::abs (dy) < 5)
                return;

            dragged = true;
            toFront (false);
            setTopLeftPosition (getX(), restTop + juce::jlimit (-slotsUp * pitch, slotsDown * pitch, dy));
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            juce::Component::SafePointer<InsertButton> safe (this);

            if (dragged)
            {
                const int slots = juce::jlimit (-slotsUp, slotsDown,
                                                (int) std::lround ((double) (getY() - restTop) / (double) pitch));
                setTopLeftPosition (getX(), restTop);

                if (slots != 0 && onReorder)
                    onReorder (slots);
            }

            if (safe != nullptr)
                juce::TextButton::mouseUp (e);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (onRightClick)
                    onRightClick();

                return;
            }

            juce::TextButton::mouseDown (e);
            dragged = false;
            restTop = getY();
        }
    };

    juce::OwnedArray<InsertButton> insertButtons;
    juce::TextButton addFxButton { "+ FX" };
    juce::TextButton moreFxButton;
    juce::TextButton outputButton { "Out: Master" };
    juce::TextButton routeButton { "Sends" };
    juce::String lastInsertSignature;

    tracktion::PluginList* getPluginList();
    void rebuildInserts();
    void openPlugin (tracktion::Plugin& plugin, juce::Component& anchor);
    void showAddFxMenu();
    void addEffect (const juce::String& id);
    void moveInsert (tracktion::Plugin& plugin, int slots);
    void showMoreMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerStrip)
};

//==============================================================================
/** All the strips side by side, master pinned on the right. */
class MixerView : public juce::Component,
                  private juce::Timer
{
public:
    explicit MixerView (EditSession& sessionToUse);
    ~MixerView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuild();
    juce::Array<tracktion::AudioTrack*> getCurrentTracks();

    EditSession& session;

    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<MixerStrip> strips;
    std::unique_ptr<MixerStrip> masterStrip;

    juce::Array<tracktion::AudioTrack*> builtTracks;

    static constexpr int stripWidth = 90;
    static constexpr int stripGap   = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerView)
};

//==============================================================================
class MixerWindow : public juce::DocumentWindow
{
public:
    explicit MixerWindow (EditSession& session);

    void closeButtonPressed() override   { setVisible (false); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerWindow)
};
