#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "InspectorParts.h"

/** Right-side panel: the selected track's volume, instrument, effects and sends. */
class TrackInspector : public juce::Component,
                       private juce::Timer
{
public:
    explicit TrackInspector (EditSession& sessionToUse);
    ~TrackInspector() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::String getSignature();
    void rebuild();
    void openInstrument();
    void showInstrumentMenu();
    void showAddEffectMenu();
    void newBus();

    EditSession& session;
    int trackIndex = -1;
    juce::String lastSignature;

    juce::Label nameLabel, hintLabel;
    juce::Label instHeading { {}, "INSTRUMENT" }, fxHeading { {}, "EFFECTS" }, sendHeading { {}, "SENDS" };
    juce::TextButton instrumentButton { "Choose instrument" }, openInstrumentButton { "Open" };
    juce::TextButton addEffectButton { "+ Add effect" }, newBusButton { "+ New bus" };
    juce::Slider volumeSlider, panSlider;

    juce::OwnedArray<ParamKnob> knobs;
    juce::OwnedArray<EffectRow> effectRows;
    juce::OwnedArray<juce::Label> sendLabels;
    juce::OwnedArray<juce::Slider> sendSliders;
    juce::Array<int> sendBuses;   // -1 means the single Reverb send used before any bus exists
    juce::Rectangle<int> instrumentCard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackInspector)
};
