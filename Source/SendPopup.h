#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** A small pop-up with one send level slider. Closes when you click away. */
class SendPopup : public juce::Component
{
public:
    SendPopup (EditSession& sessionToUse, int trackIndexToUse, int busToUse)
        : session (sessionToUse), trackIndex (trackIndexToUse), bus (busToUse)
    {
        addAndMakeVisible (label);
        label.setText ("Send to " + session.getBusName (bus), juce::dontSendNotification);

        addAndMakeVisible (slider);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
        slider.setRange (-60.0, 6.0, 0.1);
        slider.setDoubleClickReturnValue (true, -60.0);
        slider.textFromValueFunction = [] (double v)
        {
            return v <= -60.0 ? juce::String ("Off") : juce::String (v, 1) + " dB";
        };
        slider.setValue (session.getSendToBusDb (trackIndex, bus), juce::dontSendNotification);
        slider.onDragStart   = [this] { session.beginEdit(); };
        slider.onValueChange = [this] { session.setSendToBusDb (trackIndex, bus, (float) slider.getValue()); };
        slider.onDragEnd     = [this] { session.save(); };

        setSize (320, 66);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        label.setBounds (r.removeFromTop (20));
        slider.setBounds (r);
    }

    static void show (EditSession& session, int trackIndex, int bus, juce::Component& anchor)
    {
        juce::CallOutBox::launchAsynchronously (std::make_unique<SendPopup> (session, trackIndex, bus),
                                                anchor.getScreenBounds(), nullptr);
    }

private:
    EditSession& session;
    int trackIndex, bus;
    juce::Label label;
    juce::Slider slider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SendPopup)
};
