#include "RackRow.h"
#include "PatternGrid.h"

namespace te = tracktion;

static bool isAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
}

//==============================================================================
bool RackRow::isChannel()
{
    return getTrackIndex() >= 0 && ! isDrum() && (bool) track->state.getProperty ("kngChannel", false);
}

int RackRow::getChannelNote()
{
    return session.getSamplerRoot (getTrackIndex());
}

bool RackRow::hitAt (juce::Point<int> p, int& note, int& step)
{
    // Sample channels have one line of steps in the row itself
    if (isChannel())
    {
        auto lane = getLaneArea();
        const int steps = getNumSteps();

        if (p.y < 0 || p.y >= rowHeight || p.x < lane.getX() || p.x >= lane.getRight())
            return false;

        note = getChannelNote();
        step = juce::jlimit (0, steps - 1, (p.x - lane.getX()) * steps / juce::jmax (1, lane.getWidth()));
        return true;
    }

    int pad = 0;

    if (! isDrum() || ! stepAt (p, pad, step))
        return false;

    note = EditSession::getPadNote (pad);
    return true;
}

void RackRow::preview (int note)
{
    const int idx = getTrackIndex();

    if (isDrum())
    {
        session.playPad (idx, note - EditSession::getPadNote (0), 100);
        return;
    }

    if (auto* at = getAudioTrack())
        at->playGuideNote (note, te::MidiChannel (1), 100, true, true, true);
}

//==============================================================================
void RackRow::paintChannelSteps (juce::Graphics& g, te::MidiClip* clip)
{
    const int steps = getNumSteps();
    const int playStep = getPlayStep (clip);
    auto lane = getLaneArea().withHeight (rowHeight).reduced (0, 6);

    // Velocity of every hit on this channel
    int velocity[64] = {};

    if (clip != nullptr)
    {
        const double start = PatternGrid::clipStartBeat (*clip);

        for (auto* n : clip->getSequence().getNotes())
        {
            const int step = PatternGrid::beatToStep (n->getStartBeat().inBeats() - start);

            if (juce::isPositiveAndBelow (step, steps))
                velocity[step] = juce::jmax (velocity[step], n->getVelocity());
        }
    }

    for (int step = 0; step < steps; ++step)
    {
        const int x1 = lane.getX() + step * lane.getWidth() / steps;
        const int x2 = lane.getX() + (step + 1) * lane.getWidth() / steps;
        auto cell = juce::Rectangle<int> (x1, lane.getY(), x2 - x1, lane.getHeight()).reduced (2).toFloat();

        if (velocity[step] > 0)
        {
            const float v = velocity[step] / 127.0f;
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

void RackRow::paintOverChildren (juce::Graphics& g)
{
    if (hovering)
    {
        g.setColour (juce::Colour (0xff3a8ee6));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 2.0f);
    }
}

//==============================================================================
void RackRow::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int note = 0, step = 0;

    // Scroll a hit to change its velocity
    if (hitAt (e.getPosition(), note, step))
    {
        if (auto* clip = getClip (false))
        {
            if (auto* n = PatternGrid::findNote (*clip, note, step))
            {
                n->setVelocity (juce::jlimit (1, 127, n->getVelocity() + (wheel.deltaY > 0.0f ? 8 : -8)),
                                &session.getEdit().getUndoManager());

                if (clip->isLinked())
                    clip->updateLinkedClips();

                repaint();
                return;
            }
        }
    }

    // Anywhere else, let the Rack scroll
    juce::Component::mouseWheelMove (e, wheel);
}

//==============================================================================
void RackRow::showRowMenu()
{
    const int idx = getTrackIndex();

    if (idx < 0)
        return;

    auto* s = &session;
    juce::Component::SafePointer<RackRow> safe (this);
    const bool hasSampler = session.getKngSampler (idx) != nullptr;

    juce::PopupMenu menu;

    if (! isDrum())
    {
        menu.addItem ("Load Sample...", [safe] { if (safe != nullptr) safe->chooseSample(); });

        menu.addItem ("Edit in Piano Roll", [safe]
        {
            if (safe != nullptr)
                if (auto* clip = safe->getClip (true))
                    if (safe->session.onShowPianoRoll)
                        safe->session.onShowPianoRoll (*clip);
        });
    }

    if (hasSampler)
        menu.addItem ("Sampler Settings", [s, idx] { if (s->onShowSampler) s->onShowSampler (idx); });

    if (hasSampler)
    {
        const bool oneShot = session.getSamplerOneShot (idx);

        menu.addItem ("Cut at note end", true, ! oneShot, [s, idx, oneShot]
        {
            s->beginEdit();
            s->setSamplerOneShot (idx, ! oneShot);
            s->save();
        });
    }

    if (! isDrum() && ! hasSampler)
        menu.addItem ("Choose Instrument...", [safe] { if (safe != nullptr) safe->showInstrumentMenu(); });

    if (! isDrum())
    {
        menu.addSeparator();
        menu.addItem (isChannel() ? "Show as Melody" : "Show as Steps", [safe]
        {
            if (safe != nullptr)
            {
                auto& state = safe->track->state;
                const bool toSteps = ! (bool) state.getProperty ("kngChannel", false);
                state.setProperty ("kngChannel", toSteps, nullptr);

                // Notes control how long the sound plays, in both modes
                const int trackIndex = safe->getTrackIndex();
                safe->session.beginEdit();
                safe->session.setSamplerOneShot (trackIndex, false);
                safe->session.save();
                safe->repaint();
            }
        });
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void RackRow::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Load Sample", EditSession::getKitsFolder(),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult().existsAsFile())
                                  dropFile (fc.getResult());
                          });
}

void RackRow::dropFile (const juce::File& file)
{
    const int idx = getTrackIndex();

    if (idx < 0 || isDrum() || ! isAudioFile (file))
        return;

    const bool wasChannel = isChannel();

    if (session.getKngSampler (idx) == nullptr)
        session.setBuiltInSampler (idx);

    session.setSamplerSample (idx, file, session.getSamplerRoot (idx));

    // A sound channel stays one-shot and takes the sample's name
    if (wasChannel)
    {
        session.setSamplerOneShot (idx, true);
        track->setName (file.getFileNameWithoutExtension());
    }
}

//==============================================================================
bool RackRow::isInterestedInDragSource (const SourceDetails& details)
{
    if (isDrum())
        return false;

    if (auto* tree = dynamic_cast<juce::FileTreeComponent*> (details.sourceComponent.get()))
        return tree->getNumSelectedFiles() == 1 && isAudioFile (tree->getSelectedFile());

    return false;
}

void RackRow::itemDropped (const SourceDetails& details)
{
    hovering = false;

    if (auto* tree = dynamic_cast<juce::FileTreeComponent*> (details.sourceComponent.get()))
        dropFile (tree->getSelectedFile());

    repaint();
}

bool RackRow::isInterestedInFileDrag (const juce::StringArray& files)
{
    return ! isDrum() && files.size() == 1 && isAudioFile (juce::File (files[0]));
}

void RackRow::filesDropped (const juce::StringArray& files, int, int)
{
    hovering = false;

    if (! files.isEmpty())
        dropFile (juce::File (files[0]));

    repaint();
}
