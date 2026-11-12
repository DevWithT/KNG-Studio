#include "MasterStrip.h"
#include "VolumeHelpers.h"

MasterStrip::MasterStrip (EditSession& sessionToUse)
    : session (sessionToUse)
{
    addAndMakeVisible (label);
    label.setText ("Master", juce::dontSendNotification);
    label.setFont (juce::Font (juce::FontOptions (15.0f)));

    addAndMakeVisible (volumeSlider);
    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    KngVolume::setupSlider (volumeSlider);
    volumeSlider.setValue (session.getMasterVolumeDb(), juce::dontSendNotification);
    volumeSlider.onValueChange = [this] { session.setMasterVolumeDb ((float) volumeSlider.getValue()); };

    addAndMakeVisible (fxButton);
    fxButton.onClick = [this]
    {
        if (session.onShowFxChain)
            session.onShowFxChain (session.getEdit().getMasterPluginList(), "Master FX");
    };

    // Tooltips
    volumeSlider.setTooltip ("Master volume (double-click for 0 dB)");
    fxButton.setTooltip ("Open effects on the master bus");

    addAndMakeVisible (meter);
    meter.attachTo (session.getMasterMeterPlugin());
    addAndMakeVisible (loudness);
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
    loudness.setBounds (r.removeFromRight (270));
    r.removeFromRight (10);
    volumeSlider.setBounds (r.removeFromLeft (juce::jmin (360, r.getWidth())));
}
