#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** Stretch, pitch and gain for one audio clip. */
class ClipPropsComponent : public juce::Component
{
public:
    ClipPropsComponent (EditSession& sessionToUse, tracktion::AudioClipBase& clipToEdit);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void applyTempo();
    void applyPitch();

    EditSession& session;
    tracktion::AudioClipBase::Ptr clip;

    juce::Label sourceLabel { {}, "Original BPM" }, pitchLabel { {}, "Pitch (semitones)" },
                centsLabel { {}, "Fine (cents)" }, gainLabel { {}, "Clip gain" }, noteLabel;
    juce::Slider sourceBpm, pitch, cents, gain;
    juce::ToggleButton followButton { "Follow project tempo" };
    juce::TextButton resetButton { "Reset" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipPropsComponent)
};
