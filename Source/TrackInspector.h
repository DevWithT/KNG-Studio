#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "InspectorParts.h"
#include "InspectorCards.h"

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
    void showOutputMenu();

    EditSession& session;
    int trackIndex = -1;
    juce::String lastSignature;

    juce::Label nameLabel, hintLabel;
    juce::Label instHeading { {}, "INSTRUMENT" }, fxHeading { {}, "EFFECTS" }, sendHeading { {}, "SENDS" };
    juce::TextButton instrumentButton { "Choose instrument" }, openInstrumentButton { "Open" };
    juce::TextButton addEffectButton { "+ Add effect" }, sendsButton { "Sends and routing..." };
    juce::TextButton outputButton { "Output: Master" };
    juce::Label widthLabel { {}, "WIDTH" };
    juce::Slider volumeSlider, panSlider, widthSlider;

    juce::OwnedArray<ParamKnob> knobs;
    juce::OwnedArray<EffectRow> effectRows;
    juce::Rectangle<int> instrumentCard;
    std::unique_ptr<InstrumentCard> card;   // the instrument's own card, when it has an instrument

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackInspector)
};
