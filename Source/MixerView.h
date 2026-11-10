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
    juce::Label sendLabel { {}, "Reverb" }, panLabel { {}, "Pan" };
    juce::Slider sendKnob, panKnob, fader;
    LevelMeter meter;

    struct InsertButton : public juce::TextButton
    {
        std::function<void()> onRightClick;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (onRightClick)
                    onRightClick();

                return;
            }

            juce::TextButton::mouseDown (e);
        }
    };

    juce::OwnedArray<InsertButton> insertButtons;
    juce::TextButton addFxButton { "+ FX" };
    juce::ComboBox sendBusBox;
    juce::String lastInsertSignature, lastBusSignature;
    int sendBus = 0;

    tracktion::PluginList* getPluginList();
    void rebuildInserts();
    void syncSendBuses();
    void openPlugin (tracktion::Plugin& plugin, juce::Component& anchor);
    void showAddFxMenu();
    void addEffect (const juce::String& id);

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
