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

//==============================================================================
void RackRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff24282c));
    g.fillRoundedRectangle (r, 5.0f);

    // Track colour along the left edge
    if (auto* at = getAudioTrack())
    {
        const auto c = at->getColour();
        g.setColour (c.getAlpha() > 0 ? c.withAlpha (1.0f) : juce::Colour (0xff3a8ee6));
        g.fillRoundedRectangle (r.withWidth (5.0f), 2.0f);
    }

    if (getTrackIndex() < 0)
        return;

    auto* clip = getClip (false);

    if (isChannel())
    {
        paintChannelSteps (g, clip);
    }
    else if (isDrum())
    {
        paintDrumSummary (g, clip);

        if (isExpanded())
            paintDrumRows (g, clip);
    }
    else
    {
        paintMelodic (g, clip);
    }
}

void RackRow::paintDrumSummary (juce::Graphics& g, te::MidiClip* clip)
{
    const int steps = getNumSteps();
    auto lane = getLaneArea().withHeight (rowHeight).reduced (0, 9);
    const int playStep = getPlayStep (clip);

    // Which steps have any hit at all
    bool hit[128] = {};

    if (clip != nullptr)
    {
        const double start = PatternGrid::clipStartBeat (*clip);

        for (auto* n : clip->getSequence().getNotes())
        {
            const int step = PatternGrid::beatToStep (n->getStartBeat().inBeats() - start);

            if (juce::isPositiveAndBelow (step, steps))
                hit[step] = true;
        }
    }

    for (int step = 0; step < steps; ++step)
    {
        const int x1 = lane.getX() + step * lane.getWidth() / steps;
        const int x2 = lane.getX() + (step + 1) * lane.getWidth() / steps;
        auto cell = juce::Rectangle<int> (x1, lane.getY(), x2 - x1, lane.getHeight()).reduced (1).toFloat();

        g.setColour (hit[step]            ? juce::Colour (0xff3a8ee6)
                   : (step / 4) % 2 == 0  ? juce::Colour (0xff30353a)
                                          : juce::Colour (0xff3a3034));
        g.fillRoundedRectangle (cell, 2.0f);

        if (step == playStep)
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillRoundedRectangle (cell, 2.0f);
        }
    }
}

void RackRow::paintDrumRows (juce::Graphics& g, te::MidiClip* clip)
{
    const int idx = getTrackIndex();
    const int steps = getNumSteps();
    const int playStep = getPlayStep (clip);
    auto pads = getVisiblePads();

    if (pads.isEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("Drop a kit folder here from the Browser, or click Load Kit.",
                    getLocalBounds().withTrimmedTop (rowHeight).withHeight (padRowHeight).reduced (30, 0),
                    juce::Justification::centredLeft);
        return;
    }

    // Velocity of every hit, by pad and step
    int velocity[16][128] = {};

    if (clip != nullptr)
    {
        const double start = PatternGrid::clipStartBeat (*clip);

        for (auto* n : clip->getSequence().getNotes())
        {
            const int pad  = n->getNoteNumber() - EditSession::getPadNote (0);
            const int step = PatternGrid::beatToStep (n->getStartBeat().inBeats() - start);

            if (juce::isPositiveAndBelow (pad, 16) && juce::isPositiveAndBelow (step, steps))
                velocity[pad][step] = n->getVelocity();
        }
    }

    g.setFont (juce::Font (juce::FontOptions (12.0f)));

    for (int sub = 0; sub < pads.size(); ++sub)
    {
        const int pad = pads[sub];
        const int y = rowHeight + sub * padRowHeight;

        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawFittedText (session.getPadName (idx, pad), 30, y, headerWidth - 40, padRowHeight,
                          juce::Justification::centredLeft, 1);

        for (int step = 0; step < steps; ++step)
        {
            auto cell = getStepBounds (sub, step).reduced (2).toFloat();

            if (velocity[pad][step] > 0)
            {
                const float v = velocity[pad][step] / 127.0f;
                g.setColour (juce::Colour (0xff3a8ee6).interpolatedWith (juce::Colour (0xff9fd4ff), v * 0.6f));
            }
            else
            {
                g.setColour ((step / 4) % 2 == 0 ? juce::Colour (0xff353a40) : juce::Colour (0xff43363a));
            }

            g.fillRoundedRectangle (cell, 3.0f);

            if (step == playStep)
            {
                g.setColour (juce::Colours::white.withAlpha (0.16f));
                g.fillRoundedRectangle (cell, 3.0f);
            }
        }
    }
}

void RackRow::paintMelodic (juce::Graphics& g, te::MidiClip* clip)
{
    auto lane = getLaneArea().reduced (0, 5);

    g.setColour (juce::Colour (0xff1b1d20));
    g.fillRoundedRectangle (lane.toFloat(), 4.0f);

    if (clip == nullptr || clip->getSequence().getNumNotes() == 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("Click to write notes for this pattern", lane, juce::Justification::centred);
        return;
    }

    // Mini piano roll of the pattern
    const double start = PatternGrid::clipStartBeat (*clip);
    const double lengthBeats = getNumSteps() * PatternGrid::stepBeats;
    const auto range = clip->getSequence().getNoteNumberRange();
    const int lo = range.getStart() - 1;
    const float span = (float) juce::jmax (8, range.getEnd() + 1 - lo);

    g.setColour (juce::Colour (0xff3a8ee6));

    for (auto* n : clip->getSequence().getNotes())
    {
        const double b1 = n->getStartBeat().inBeats() - start;
        const double b2 = b1 + n->getLengthBeats().inBeats();

        if (b2 <= 0.0 || b1 >= lengthBeats)
            continue;

        const float x1 = lane.getX() + (float) (juce::jmax (0.0, b1) / lengthBeats) * lane.getWidth();
        const float x2 = lane.getX() + (float) (juce::jmin (lengthBeats, b2) / lengthBeats) * lane.getWidth();
        const float y  = lane.getY() + 2.0f + (1.0f - (n->getNoteNumber() - lo) / span) * (lane.getHeight() - 6.0f);

        g.fillRoundedRectangle (x1, y, juce::jmax (2.0f, x2 - x1 - 1.0f), 3.0f, 1.0f);
    }

    // Playhead
    const int playStep = getPlayStep (clip);

    if (playStep >= 0)
    {
        const double beat = clip->getContentBeatAtTime (session.getTransport().getPosition()).inBeats() - start;
        const int x = lane.getX() + juce::roundToInt (beat / lengthBeats * lane.getWidth());
        g.setColour (juce::Colours::yellow);
        g.fillRect (x, lane.getY(), 2, lane.getHeight());
    }
}

void RackRow::resized()
{
    auto r = juce::Rectangle<int> (0, 0, headerWidth, rowHeight).reduced (8, 6);
    r.removeFromLeft (2);

    expandButton.setBounds (r.removeFromLeft (22));
    r.removeFromLeft (4);
    instrumentButton.setBounds (r.removeFromRight (70));
    r.removeFromRight (4);
    panKnob.setBounds (r.removeFromRight (24));
    volKnob.setBounds (r.removeFromRight (24));
    r.removeFromRight (4);
    soloButton.setBounds (r.removeFromRight (22));
    r.removeFromRight (2);
    muteButton.setBounds (r.removeFromRight (22));
    r.removeFromRight (4);
    nameLabel.setBounds (r);
}

//==============================================================================
void RackRow::mouseDown (const juce::MouseEvent& e)
{
    const int idx = getTrackIndex();

    if (idx < 0)
        return;

    const auto p = e.getPosition();
    auto lane = getLaneArea();
    const bool rightButton = e.mods.isRightButtonDown() || e.mods.isPopupMenu();

    // Clicking a row shows its track in the inspector on the right
    session.selectTrack (idx);

    // Left side: right-click for the menu, left-click to hear the sound
    if (p.x < lane.getX())
    {
        if (rightButton && p.y < rowHeight)
        {
            showRowMenu();
            return;
        }

        if (isDrum() && isExpanded() && p.y >= rowHeight)
        {
            auto pads = getVisiblePads();
            const int sub = (p.y - rowHeight) / padRowHeight;

            if (juce::isPositiveAndBelow (sub, pads.size()))
                session.playPad (idx, pads[sub], 110);
        }
        else if (isChannel() && ! rightButton)
        {
            preview (getChannelNote());
        }

        return;
    }

    // Melodic rows open the piano roll for this pattern
    if (! isDrum() && ! isChannel())
    {
        if (auto* clip = getClip (true))
            if (session.onShowPianoRoll)
                session.onShowPianoRoll (*clip);

        return;
    }

    // Clicking the summary strip opens a closed drum row
    if (isDrum() && p.y < rowHeight)
    {
        if (! isExpanded())
            setExpanded (true);

        return;
    }

    int note = 0, step = 0;

    if (! hitAt (p, note, step))
        return;

    auto* clip = getClip (true);

    if (clip == nullptr)
        return;

    paintOn = ! rightButton && PatternGrid::findNote (*clip, note, step) == nullptr;

    session.beginEdit();
    painting = true;
    lastNote = note;
    lastStep = step;

    PatternGrid::setStep (*clip, note, step, paintOn, &session.getEdit().getUndoManager());

    if (paintOn)
        preview (note);

    repaint();
}

void RackRow::mouseDrag (const juce::MouseEvent& e)
{
    if (! painting)
        return;

    int note = 0, step = 0;

    if (hitAt (e.getPosition(), note, step) && (note != lastNote || step != lastStep))
    {
        lastNote = note;
        lastStep = step;

        if (auto* clip = getClip (false))
            PatternGrid::setStep (*clip, note, step, paintOn, &session.getEdit().getUndoManager());

        repaint();
    }
}

void RackRow::mouseUp (const juce::MouseEvent&)
{
    if (! painting)
        return;

    painting = false;

    if (auto* clip = getClip (false))
    {
        // Sound rows: each hit rings until the next one
        if (isChannel())
            PatternGrid::fillToNext (*clip, getChannelNote(), getNumSteps(), &session.getEdit().getUndoManager());

        // Push the change to every linked copy on the timeline
        if (clip->isLinked())
            clip->updateLinkedClips();
    }

    session.save();
}

