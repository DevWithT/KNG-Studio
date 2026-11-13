#include "RackRow.h"
#include "PatternGrid.h"
#include "VolumeHelpers.h"
#include "KngSamplerPlugin.h"
#include "KngSynthPlugin.h"
#include <map>

namespace te = tracktion;

//==============================================================================
RackRow::RackRow (EditSession& sessionToUse, te::Track& trackToUse)
    : session (sessionToUse), track (&trackToUse)
{
    for (auto* c : std::initializer_list<juce::Component*> { &expandButton, &nameLabel, &muteButton, &soloButton,
                                                             &volKnob, &panKnob, &instrumentButton })
        addAndMakeVisible (c);

    nameLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    nameLabel.setInterceptsMouseClicks (false, false);

    expandButton.onClick = [this] { setExpanded (! isExpanded()); };

    for (auto* b : { &muteButton, &soloButton })
        b->setClickingTogglesState (false);

    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::orange);
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::gold);

    muteButton.onClick = [this] { if (auto* at = getAudioTrack()) at->setMute (! at->isMuted (false)); };
    soloButton.onClick = [this] { if (auto* at = getAudioTrack()) at->setSolo (! at->isSolo (false)); };

    for (auto* k : { &volKnob, &panKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->setPopupDisplayEnabled (true, true, this);
    }

    KngVolume::setupSlider (volKnob);
    volKnob.onValueChange = [this] { session.setTrackVolumeDb (getTrackIndex(), (float) volKnob.getValue()); };

    panKnob.setRange (-1.0, 1.0, 0.01);
    panKnob.setDoubleClickReturnValue (true, 0.0);
    panKnob.onValueChange = [this]
    {
        if (auto* at = getAudioTrack())
            if (auto* vp = at->getVolumePlugin())
                vp->setPan ((float) panKnob.getValue());
    };

    instrumentButton.onClick = [this] { openInstrument(); };

    // Record from your MIDI keyboard; the take becomes a pattern block in the Playlist
    addAndMakeVisible (armButton);
    armButton.setClickingTogglesState (false);
    armButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red);
    armButton.setTooltip ("Arm to record from your MIDI keyboard (in Song mode). The take becomes a pattern block.");
    armButton.onClick = [this]
    {
        const int idx = getTrackIndex();

        if (idx >= 0)
            session.setArmed (idx, ! session.isArmed (idx));
    };

    // Tooltips
    expandButton.setTooltip ("Show or hide this row's pads");
    muteButton.setTooltip ("Mute");
    soloButton.setTooltip ("Solo");
    volKnob.setTooltip ("Volume (double-click for 0 dB)");
    panKnob.setTooltip ("Pan (double-click to center)");
    instrumentButton.setTooltip ("Open this sound's settings. Right-click the row for more options.");

    syncControls();
    startTimerHz (12);
}

RackRow::~RackRow()
{
    stopTimer();
}

//==============================================================================
int RackRow::getTrackIndex()
{
    for (int i = 0; i < session.getNumTracks(); ++i)
        if (session.getTrack (i) == track.get())
            return i;

    return -1;
}

te::AudioTrack* RackRow::getAudioTrack()
{
    return dynamic_cast<te::AudioTrack*> (track.get());
}

bool RackRow::isDrum()
{
    const int idx = getTrackIndex();
    return idx >= 0 && session.isDrumTrack (idx);
}

bool RackRow::isExpanded()
{
    return (bool) track->state.getProperty ("kngRackOpen", true);
}

void RackRow::setExpanded (bool shouldBeOpen)
{
    track->state.setProperty ("kngRackOpen", shouldBeOpen, nullptr);

    if (onLayoutChanged)
        onLayoutChanged();
}

juce::Array<int> RackRow::getVisiblePads()
{
    juce::Array<int> pads;
    const int idx = getTrackIndex();

    for (int p = 0; p < 16; ++p)
        if (session.getPadName (idx, p).isNotEmpty())
            pads.add (p);

    return pads;
}

int RackRow::getNumSteps()
{
    return session.getPatternSteps (session.getCurrentPattern());
}

te::MidiClip* RackRow::getClip (bool createIfMissing)
{
    return session.getPatternClip (getTrackIndex(), session.getCurrentPattern(), createIfMissing);
}

int RackRow::getIdealHeight()
{
    if (! isDrum() || ! isExpanded())
        return rowHeight;

    return rowHeight + juce::jmax (1, getVisiblePads().size()) * padRowHeight + 6;
}

//==============================================================================
juce::Rectangle<int> RackRow::getLaneArea() const
{
    return getLocalBounds().withTrimmedLeft (headerWidth).reduced (6, 0);
}

juce::Rectangle<int> RackRow::getStepBounds (int subRow, int step)
{
    auto lane = getLaneArea();
    const int steps = getNumSteps();

    const int x1 = lane.getX() + step * lane.getWidth() / steps;
    const int x2 = lane.getX() + (step + 1) * lane.getWidth() / steps;

    return { x1, rowHeight + subRow * padRowHeight, x2 - x1, padRowHeight };
}

bool RackRow::stepAt (juce::Point<int> p, int& pad, int& step)
{
    auto pads = getVisiblePads();
    const int sub = (p.y - rowHeight) / padRowHeight;
    auto lane = getLaneArea();

    if (p.y < rowHeight || ! juce::isPositiveAndBelow (sub, pads.size()) || ! lane.contains (p.x, lane.getCentreY()))
        return false;

    pad = pads[sub];
    step = juce::jlimit (0, getNumSteps() - 1, (p.x - lane.getX()) * getNumSteps() / juce::jmax (1, lane.getWidth()));
    return true;
}

int RackRow::getPlayStep (te::MidiClip* clip)
{
    if (clip == nullptr || ! session.getTransport().isPlaying())
        return -1;

    const double beat = clip->getContentBeatAtTime (session.getTransport().getPosition()).inBeats()
                      - PatternGrid::clipStartBeat (*clip);

    return (beat >= 0.0 && beat < getNumSteps() * PatternGrid::stepBeats) ? (int) (beat / PatternGrid::stepBeats) : -1;
}

//==============================================================================
void RackRow::timerCallback()
{
    syncControls();
    repaint();
}

void RackRow::syncControls()
{
    auto* at = getAudioTrack();
    const int idx = getTrackIndex();

    if (at == nullptr || idx < 0)
        return;

    if (nameLabel.getText() != at->getName())
        nameLabel.setText (at->getName(), juce::dontSendNotification);

    muteButton.setToggleState (at->isMuted (false), juce::dontSendNotification);
    soloButton.setToggleState (at->isSolo (false), juce::dontSendNotification);
    armButton.setToggleState (session.isArmed (idx), juce::dontSendNotification);

    if (auto* vp = at->getVolumePlugin())
    {
        if (! volKnob.isMouseButtonDown())
            volKnob.setValue (session.getTrackVolumeDb (idx), juce::dontSendNotification);

        if (! panKnob.isMouseButtonDown())
            panKnob.setValue (vp->getPan(), juce::dontSendNotification);
    }

    auto* inst = session.getInstrument (idx);
    const auto text = isChannel()                                        ? juce::String ("Sample")
                    : isDrum()                                           ? juce::String (getVisiblePads().isEmpty() ? "Load Kit" : "Kit")
                    : dynamic_cast<KngSamplerPlugin*> (inst) != nullptr ? juce::String ("Sampler")
                    : dynamic_cast<KngSynthPlugin*> (inst) != nullptr   ? juce::String ("Synth")
                    : inst != nullptr                                    ? inst->getName()
                                                                         : juce::String ("+ Inst");

    if (instrumentButton.getButtonText() != text)
        instrumentButton.setButtonText (text);

    expandButton.setVisible (isDrum());
    expandButton.setButtonText (juce::String (juce::CharPointer_UTF8 (isExpanded() ? "\xe2\x96\xbe" : "\xe2\x96\xb8")));
}

void RackRow::openInstrument()
{
    const int idx = getTrackIndex();

    if (idx < 0)
        return;

    if (isDrum())
    {
        // The Drum Kit tab is where kits load and pads get tweaked
        if (session.onShowDrumRack)
            session.onShowDrumRack (idx);

        return;
    }

    auto* inst = session.getInstrument (idx);

    if (dynamic_cast<KngSamplerPlugin*> (inst) != nullptr || dynamic_cast<KngSynthPlugin*> (inst) != nullptr)
    {
        if (session.onShowSampler)
            session.onShowSampler (idx);

        return;
    }

    // Other KNG instruments (KNG 808) open their panel at the bottom
    if (inst != nullptr && inst->getPluginType().startsWith ("kng") && session.onShowPluginEditor)
    {
        session.onShowPluginEditor (*inst);
        return;
    }

    if (inst != nullptr)
    {
        inst->showWindowExplicitly();
        return;
    }

    showInstrumentMenu();
}

void RackRow::showInstrumentMenu()
{
    auto* s = &session;
    const int idx = getTrackIndex();

    juce::PopupMenu menu;
    menu.addItem ("KNG Sampler (built-in)", [s, idx] { s->setBuiltInSampler (idx); });
    menu.addItem ("KNG Synth (built-in)", [s, idx] { s->setBuiltInSynth (idx); });
    menu.addItem ("KNG 808 (built-in)", [s, idx] { s->setInstrumentById (idx, "kng808"); });
    menu.addSeparator();

    std::map<juce::String, juce::PopupMenu> byMaker;

    for (auto& d : session.getEdit().engine.getPluginManager().knownPluginList.getTypes())
    {
        if (! d.isInstrument)
            continue;

        const auto maker = d.manufacturerName.isNotEmpty() ? d.manufacturerName : juce::String ("Other");
        byMaker[maker].addItem (d.name, [s, idx, d] { s->setInstrument (idx, d); });
    }

    if (byMaker.empty())
        menu.addItem ("No VST instruments yet. Use File > Scan Plugins.", false, false, nullptr);

    for (auto& [maker, sub] : byMaker)
        menu.addSubMenu (maker, sub);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&instrumentButton));
}

