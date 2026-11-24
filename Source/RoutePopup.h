#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "KngTheme.h"
#include "RouteMenu.h"

/** The Routing pop-up for one lane: where its sound goes (Output) and its sends.
    Each send has a level, a Pre / Post fader switch, a mute and a remove button. */
class RoutePopup : public juce::Component,
                   private juce::Timer
{
public:
    RoutePopup (EditSession& sessionToUse, int trackIndexToUse)
        : session (sessionToUse), trackIndex (trackIndexToUse)
    {
        addAndMakeVisible (title);
        title.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));

        addAndMakeVisible (outputLabel);
        outputLabel.setText ("Output", juce::dontSendNotification);

        addAndMakeVisible (outputButton);
        outputButton.setTooltip ("Where this lane's sound goes. Master is the normal route. Pick a bus or another lane to feed a group.");
        outputButton.onClick = [this]
        {
            if (auto* t = session.getTrack (trackIndex))
            {
                juce::PopupMenu menu;
                KngRoute::addItems (menu, session, *t);
                menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&outputButton));
            }
        };

        addAndMakeVisible (sendsLabel);
        sendsLabel.setText ("Sends", juce::dontSendNotification);

        addAndMakeVisible (addBox);
        addBox.setTooltip ("Send a copy of this lane to a bus, for reverb, delay or a headphone mix");
        addBox.onChange = [this] { addPicked(); };

        rebuild();
        startTimerHz (4);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        title.setBounds (r.removeFromTop (22));
        r.removeFromTop (8);

        auto out = r.removeFromTop (24);
        outputLabel.setBounds (out.removeFromLeft (60));
        outputButton.setBounds (out);
        r.removeFromTop (10);

        sendsLabel.setBounds (r.removeFromTop (18));
        r.removeFromTop (4);

        for (auto* row : rows)
        {
            row->setBounds (r.removeFromTop (26));
            r.removeFromTop (4);
        }

        addBox.setBounds (r.removeFromTop (24));
    }

    static void show (EditSession& session, int trackIndex, juce::Component& anchor)
    {
        juce::CallOutBox::launchAsynchronously (std::make_unique<RoutePopup> (session, trackIndex),
                                                anchor.getScreenBounds(), nullptr);
    }

private:
    //==========================================================================
    struct SendRow : public juce::Component
    {
        SendRow (EditSession& s, int idx, int busToUse, std::function<void()> onRemoved)
            : session (s), trackIndex (idx), bus (busToUse), removedCallback (std::move (onRemoved))
        {
            addAndMakeVisible (busLabel);
            busLabel.setText (session.getBusName (bus), juce::dontSendNotification);
            busLabel.setMinimumHorizontalScale (0.8f);

            addAndMakeVisible (slider);
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
            slider.setRange (-60.0, 6.0, 0.1);
            slider.setDoubleClickReturnValue (true, 0.0);
            slider.textFromValueFunction = [] (double v)
            {
                return v <= -60.0 ? juce::String ("Off") : juce::String (v, 1) + " dB";
            };
            slider.setValue (session.getSendToBusDb (trackIndex, bus), juce::dontSendNotification);
            slider.onDragStart   = [this] { session.beginEdit(); };
            slider.onValueChange = [this] { session.setSendToBusDb (trackIndex, bus, (float) slider.getValue()); };
            slider.onDragEnd     = [this] { session.save(); };

            for (auto* b : { &modeButton, &muteButton, &removeButton })
            {
                addAndMakeVisible (b);
                b->setClickingTogglesState (false);
            }

            muteButton.setColour (juce::TextButton::buttonOnColourId, KngTheme::accent);
            muteButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            removeButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")));

            modeButton.setTooltip ("Post: the send follows this lane's fader. Pre: the send ignores the fader (good for headphone mixes).");
            muteButton.setTooltip ("Mute this send");
            removeButton.setTooltip ("Remove this send");

            modeButton.onClick = [this]
            {
                session.setSendPreFader (trackIndex, bus, ! session.isSendPreFader (trackIndex, bus));
                updateButtons();
            };

            muteButton.onClick = [this]
            {
                session.setSendMuted (trackIndex, bus, ! session.isSendMuted (trackIndex, bus));
                updateButtons();
            };

            removeButton.onClick = [this]
            {
                session.removeSend (trackIndex, bus);

                if (removedCallback)
                    removedCallback();
            };

            updateButtons();
        }

        void updateButtons()
        {
            modeButton.setButtonText (session.isSendPreFader (trackIndex, bus) ? "Pre" : "Post");
            muteButton.setToggleState (session.isSendMuted (trackIndex, bus), juce::dontSendNotification);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            removeButton.setBounds (r.removeFromRight (26));
            r.removeFromRight (4);
            muteButton.setBounds (r.removeFromRight (28));
            r.removeFromRight (4);
            modeButton.setBounds (r.removeFromRight (46));
            r.removeFromRight (6);
            busLabel.setBounds (r.removeFromLeft (84));
            slider.setBounds (r);
        }

        EditSession& session;
        const int trackIndex, bus;
        std::function<void()> removedCallback;

        juce::Label busLabel;
        juce::Slider slider;
        juce::TextButton modeButton { "Post" }, muteButton { "M" }, removeButton;
    };

    //==========================================================================
    void timerCallback() override
    {
        if (addBox.isPopupActive())
            return;

        if (makeSignature() != signature)
            rebuild();
    }

    juce::String makeSignature()
    {
        juce::String sig;

        for (int b : session.getSendBuses (trackIndex))
            sig << b << ",";

        if (auto* t = session.getTrack (trackIndex))
            sig << "|" << KngRoute::currentName (*t) << "|";

        for (int b : session.getBusTargets (trackIndex))
            sig << b << ",";

        sig << "|" << session.getNumTracks();
        return sig;
    }

    // Row buttons call this; the rebuild waits a beat so a row is never deleted inside its own click
    void requestRebuild()
    {
        juce::Component::SafePointer<RoutePopup> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->rebuild(); });
    }

    void rebuild()
    {
        rows.clear();

        if (auto* t = session.getTrack (trackIndex))
            title.setText ("Routing: " + t->getName(), juce::dontSendNotification);

        const auto targets = session.getBusTargets (trackIndex);
        const auto sent = session.getSendBuses (trackIndex);

        if (auto* t = session.getTrack (trackIndex))
            outputButton.setButtonText ("Output: " + KngRoute::currentName (*t));

        addBox.clear (juce::dontSendNotification);
        addBox.addItem ("Add send...", 1);

        for (int b : targets)
            if (! sent.contains (b))
                addBox.addItem (session.getBusName (b), 100 + b);

        addBox.addItem ("New bus...", 10000);
        addBox.setSelectedId (1, juce::dontSendNotification);

        for (int b : sent)
        {
            auto* row = rows.add (new SendRow (session, trackIndex, b, [this] { requestRebuild(); }));
            addAndMakeVisible (row);
        }

        signature = makeSignature();
        setSize (400, 130 + 30 * rows.size());
        resized();
    }

    void addPicked()
    {
        const int id = addBox.getSelectedId();

        if (id >= 100 && id < 10000)
        {
            session.setSendToBusDb (trackIndex, id - 100, -6.0f);
            requestRebuild();
            return;
        }

        if (id == 10000)
        {
            auto* s = &session;
            const int idx = trackIndex;
            auto* w = new juce::AlertWindow ("New Bus", "Name this bus:", juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("name", s->getBusNumbers().isEmpty() ? "Reverb Bus" : "New Bus");
            w->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

            w->enterModalState (true, juce::ModalCallbackFunction::create ([s, idx, w] (int result)
            {
                const auto name = w->getTextEditorContents ("name").trim();

                if (result == 1 && name.isNotEmpty())
                {
                    const int bus = s->addBus (name);

                    if (bus >= 0)
                        s->setSendToBusDb (idx, bus, -6.0f);
                }
            }), true);

            addBox.setSelectedId (1, juce::dontSendNotification);
        }
    }

    EditSession& session;
    const int trackIndex;
    juce::String signature;

    juce::Label title, outputLabel, sendsLabel;
    juce::TextButton outputButton;
    juce::ComboBox addBox;
    juce::OwnedArray<SendRow> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutePopup)
};
