#include "PianoRoll.h"
#include "TheoryHelper.h"
#include "KngSamplerPlugin.h"
#include "KngSynthPlugin.h"

namespace te = tracktion;

//==============================================================================
// Notes

bool PianoRoll::isValid (te::MidiNote* n) const
{
    return n != nullptr && clip->getSequence().getNotes().contains (n);
}

void PianoRoll::cleanSelection()
{
    for (int i = selected.size(); --i >= 0;)
        if (! isValid (selected[i]))
            selected.remove (i);
}

te::MidiNote* PianoRoll::findNoteAt (juce::Point<int> p, bool& onRightEdge)
{
    onRightEdge = false;

    for (auto* n : clip->getSequence().getNotes())
    {
        auto r = getNoteRect (*n);

        if (r.contains (p))
        {
            onRightEdge = r.getWidth() > 10 && p.x >= r.getRight() - 6;
            return n;
        }
    }

    return nullptr;
}

void PianoRoll::previewNotes (const juce::Array<int>& notes, int velocity)
{
    auto* at = dynamic_cast<te::AudioTrack*> (clip->getClipTrack());

    if (at == nullptr || notes.isEmpty())
        return;

    // The 5th argument (forceNote) must be true or the engine skips guide notes
    for (int i = 0; i < notes.size(); ++i)
        at->playGuideNote (notes[i], te::MidiChannel (1), velocity, i == 0, true, true);

    lastPreviewNote = notes.getLast();
}

void PianoRoll::applyVelocity (juce::Point<int> p)
{
    auto va = getVelocityArea();
    const int vel = juce::jlimit (1, 127, juce::roundToInt ((va.getBottom() - p.y) / (double) va.getHeight() * 127.0));

    // Nearest note start within a few pixels
    te::MidiNote* best = nullptr;
    int bestDist = 9;

    for (auto* n : clip->getSequence().getNotes())
    {
        const int d = std::abs (beatToX (n->getStartBeat().inBeats()) - p.x);

        if (d < bestDist)
        {
            best = n;
            bestDist = d;
        }
    }

    if (best == nullptr)
        return;

    if (selected.contains (best))
    {
        for (auto* n : selected)
            n->setVelocity (vel, um());
    }
    else
    {
        best->setVelocity (vel, um());
    }

    repaint();
}

void PianoRoll::quantise()
{
    const double g = getGridBeats();

    if (g <= 0.0)
        return;

    cleanSelection();
    session.beginEdit();

    auto notes = selected.isEmpty() ? clip->getSequence().getNotes() : selected;

    for (auto* n : notes)
    {
        const double start = std::round (n->getStartBeat().inBeats() / g) * g;
        n->setStartAndLength (te::BeatPosition::fromBeats (start), n->getLengthBeats(), um());
    }

    session.save();
    repaint();
}

void PianoRoll::deleteSelected()
{
    cleanSelection();

    if (selected.isEmpty())
        return;

    session.beginEdit();
    auto toRemove = selected;
    selected.clear();

    for (auto* n : toRemove)
        if (isValid (n))
            clip->getSequence().removeNote (*n, um());

    session.save();
    repaint();
}

void PianoRoll::eraseAt (juce::Point<int> p)
{
    bool edge = false;

    if (auto* n = findNoteAt (p, edge))
    {
        selected.removeFirstMatchingValue (n);
        clip->getSequence().removeNote (*n, um());
        repaint();
    }
}

//==============================================================================
void PianoRoll::toggleSlideOnSelected()
{
    cleanSelection();

    if (selected.isEmpty())
        return;

    session.beginEdit();
    auto& seq = clip->getSequence();

    for (auto* n : selected)
    {
        const double ns = n->getStartBeat().inBeats();
        te::MidiNote* prev = nullptr;

        // The note right before this one, on any pitch
        for (auto* m : seq.getNotes())
            if (m != n && m->getStartBeat().inBeats() < ns - 1.0e-6
                 && (prev == nullptr || m->getStartBeat() > prev->getStartBeat()))
                prev = m;

        if (prev == nullptr)
            continue;

        const double ps = prev->getStartBeat().inBeats();
        const bool sliding = ps + prev->getLengthBeats().inBeats() > ns + 1.0e-6;

        // Slide on: the note before rings a little past this one's start, so it glides in.
        // Slide off: it stops exactly where this one starts.
        const double newLength = sliding ? ns - ps : ns - ps + 0.125;
        prev->setStartAndLength (prev->getStartBeat(), te::BeatDuration::fromBeats (newLength), um());
    }

    // Slides need one note at a time, with glide
    if (auto* track = dynamic_cast<te::AudioTrack*> (clip->getClipTrack()))
    {
        if (auto* sampler = track->pluginList.findFirstPluginOfType<KngSamplerPlugin>())
        {
            sampler->state.setProperty (KngSamplerPlugin::monoProp, true, um());

            if ((float) sampler->state.getProperty (KngSamplerPlugin::glideProp, 0.0f) < 1.0f)
                sampler->state.setProperty (KngSamplerPlugin::glideProp, 60.0f, um());

            sampler->syncFromState();
        }

        if (auto* synth = track->pluginList.findFirstPluginOfType<KngSynthPlugin>())
        {
            if (synth->getParam (KngSynth::voiceMode)->getCurrentValue() < 0.5f)
                synth->getParam (KngSynth::voiceMode)->setParameter (1.0f, juce::sendNotification);

            if (synth->getParam (KngSynth::glide)->getCurrentValue() < 0.01f)
                synth->getParam (KngSynth::glide)->setParameter (0.06f, juce::sendNotification);
        }
    }

    if (clip->isLinked())
        clip->updateLinkedClips();

    session.save();
    repaint();
}
