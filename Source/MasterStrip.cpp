#include "MasterStrip.h"

MasterStrip::MasterStrip (EditSession& sessionToUse)
    : session (sessionToUse)
{
    addAndMakeVisible (label);
    label.setText ("Master", juce::dontSendNotification);
    label.setFont (juce::Font (juce::FontOptions (15.0f)));

    addAndMakeVisible (volumeSlider);
    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    volumeSlider.setRange (-60.0, 6.0, 0.1);
    volumeSlider.setTextValueSuffix (" dB");
    volumeSlider.setDoubleClickReturnValue (true, 0.0);
    volumeSlider.setValue (session.getMasterVolumeDb(), juce::dontSendNotification);
    volumeSlider.onValueChange = [this] { session.setMasterVolumeDb ((float) volumeSlider.getValue()); };

    addAndMakeVisible (fxButton);
    fxButton.onClick = [this]
    {
        if (session.onShowFxChain)
            session.onShowFxChain (session.getEdit().getMasterPluginList(), "Master FX");
    };

    addAndMakeVisible (meter);
    meter.attachTo (session.getMasterMeterPlugin());
}

void MasterStrip::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff3a3f44));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

void MasterStrip::resized()
{
    auto r = getLocalBounds().reduced (6);

    label.setBounds (r.removeFromLeft (80));
    fxButton.setBounds (r.removeFromLeft (44));
    r.removeFromLeft (10);
    meter.setBounds (r.removeFromRight (200).reduced (0, 4));
    r.removeFromRight (10);
    volumeSlider.setBounds (r.removeFromLeft (juce::jmin (360, r.getWidth())));
}
