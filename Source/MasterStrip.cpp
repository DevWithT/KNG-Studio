#include "MasterStrip.h"
#include "VolumeHelpers.h"
#include "KngTheme.h"

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

    // One click: Width, Comp, Air and Limiter, set for streaming or loud
    addAndMakeVisible (chainButton);
    chainButton.setTooltip ("Add a finishing chain to the master. Then nudge the Limiter's Gain until LUFS reads your target.");
    chainButton.onClick = [this]
    {
        auto* s = &session;
        juce::PopupMenu menu;
        menu.addSectionHeader ("Width, Comp, Air and Limiter on the master");
        menu.addItem ("Streaming Ready (about -14 LUFS)", [s] { s->applyMasterChain (1); });
        menu.addItem ("Loud (about -9 LUFS, trap and club)", [s] { s->applyMasterChain (2); });
        menu.addSeparator();
        menu.addItem ("Remove Master Chain", [s] { s->applyMasterChain (0); });
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&chainButton));
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
    g.fillAll (KngTheme::panel);
    g.setColour (KngTheme::line);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void MasterStrip::resized()
{
    auto r = getLocalBounds().reduced (6);

    label.setBounds (r.removeFromLeft (80));
    fxButton.setBounds (r.removeFromLeft (44));
    r.removeFromLeft (6);
    chainButton.setBounds (r.removeFromLeft (110));
    r.removeFromLeft (10);
    meter.setBounds (r.removeFromRight (200).reduced (0, 4));
    r.removeFromRight (10);
    loudness.setBounds (r.removeFromRight (270));
    r.removeFromRight (10);
    volumeSlider.setBounds (r.removeFromLeft (juce::jmin (360, r.getWidth())));
}
