#include "ClipProps.h"

ClipPropsComponent::ClipPropsComponent (EditSession& sessionToUse, tracktion::AudioClipBase& clipToEdit)
    : session (sessionToUse), clip (&clipToEdit)
{
    for (auto* c : std::initializer_list<juce::Component*> { &sourceLabel, &sourceBpm, &followButton,
                                                             &pitchLabel, &pitch, &centsLabel, &cents,
                                                             &gainLabel, &gain, &resetButton, &noteLabel })
        addAndMakeVisible (c);

    noteLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    noteLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    noteLabel.setText ("Project is " + juce::String (session.getBpm(), 1)
                       + " BPM. Set the clip's original BPM and tick Follow to lock it to the project.",
                       juce::dontSendNotification);

    // Tempo
    sourceBpm.setSliderStyle (juce::Slider::IncDecButtons);
    sourceBpm.setRange (20.0, 400.0, 0.1);
    sourceBpm.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 70, 24);
    sourceBpm.setValue (session.getClipSourceBpm (*clip), juce::dontSendNotification);
    sourceBpm.onValueChange = [this] { if (followButton.getToggleState()) applyTempo(); };

    followButton.setToggleState (clip->getAutoTempo(), juce::dontSendNotification);
    followButton.onClick = [this] { applyTempo(); };

    // Pitch, split into whole semitones and cents
    const float pc = clip->getPitchChange();
    const int semis = (int) std::round (pc);

    for (auto* sl : { &pitch, &cents, &gain })
    {
        sl->setSliderStyle (juce::Slider::LinearHorizontal);
        sl->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
        sl->onDragStart = [this] { session.beginEdit(); };
        sl->onDragEnd   = [this] { session.save(); };
    }

    pitch.setRange (-12.0, 12.0, 1.0);
    pitch.setDoubleClickReturnValue (true, 0.0);
    pitch.setValue (semis, juce::dontSendNotification);
    pitch.onValueChange = [this] { applyPitch(); };

    cents.setRange (-100.0, 100.0, 1.0);
    cents.setDoubleClickReturnValue (true, 0.0);
    cents.setValue (std::round ((pc - (float) semis) * 100.0f), juce::dontSendNotification);
    cents.onValueChange = [this] { applyPitch(); };

    // Gain
    gain.setRange (-24.0, 12.0, 0.1);
    gain.setTextValueSuffix (" dB");
    gain.setDoubleClickReturnValue (true, 0.0);
    gain.setValue (clip->getGainDB(), juce::dontSendNotification);
    gain.onValueChange = [this] { session.setClipGain (*clip, (float) gain.getValue()); };

    resetButton.onClick = [this]
    {
        session.setClipFollowTempo (*clip, false, sourceBpm.getValue());
        session.setClipPitch (*clip, 0.0f);
        session.setClipGain (*clip, 0.0f);
        session.save();

        followButton.setToggleState (false, juce::dontSendNotification);
        pitch.setValue (0.0, juce::dontSendNotification);
        cents.setValue (0.0, juce::dontSendNotification);
        gain.setValue (0.0, juce::dontSendNotification);
    };

    setSize (440, 300);
}

void ClipPropsComponent::applyTempo()
{
    session.setClipFollowTempo (*clip, followButton.getToggleState(), sourceBpm.getValue());
}

void ClipPropsComponent::applyPitch()
{
    session.setClipPitch (*clip, (float) (pitch.getValue() + cents.getValue() / 100.0));
}

void ClipPropsComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2b2f33));
}

void ClipPropsComponent::resized()
{
    auto r = getLocalBounds().reduced (12);

    auto row = [&r] { auto x = r.removeFromTop (26); r.removeFromTop (8); return x; };

    auto r1 = row();
    sourceLabel.setBounds (r1.removeFromLeft (130));
    sourceBpm.setBounds (r1.removeFromLeft (150));

    followButton.setBounds (row());

    auto r3 = row();
    pitchLabel.setBounds (r3.removeFromLeft (130));
    pitch.setBounds (r3);

    auto r4 = row();
    centsLabel.setBounds (r4.removeFromLeft (130));
    cents.setBounds (r4);

    auto r5 = row();
    gainLabel.setBounds (r5.removeFromLeft (130));
    gain.setBounds (r5);

    resetButton.setBounds (row().removeFromLeft (90));
    noteLabel.setBounds (r);
}
