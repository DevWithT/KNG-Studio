#pragma once

#include "EditSession.h"
#include "PitchTracker.h"

/** Convert to MIDI: runs the pitch tracker on an audio clip and puts the notes on a new KNG Synth track. */
namespace KngAudioToMidi
{
    /** Returns how many notes were made, 0 if none were found, or -1 if the clip could not be converted. */
    inline int convert (EditSession& session, tracktion::AudioClipBase& clip)
    {
        namespace te = tracktion;

        if (session.getTransport().isRecording())
            return -1;

        const auto source = clip.getAudioFile().getFile();

        if (! source.existsAsFile())
            return -1;

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));

        if (reader == nullptr)
            return -1;

        const auto pos = clip.getPosition();
        const double clipStart = pos.getStart().inSeconds();
        const auto notes = KngPitch::findNotes (*reader, pos.getOffset().inSeconds(), pos.getLength().inSeconds());

        if (notes.empty())
            return 0;

        const auto name = clip.getName().isNotEmpty() ? clip.getName() : source.getFileNameWithoutExtension();
        const int index = session.addInstrumentTrack ("kngsynth", name + " (MIDI)");
        auto* track = session.getTrack (index);

        if (track == nullptr)
            return -1;

        session.beginEdit();
        auto midi = track->insertMIDIClip (te::TimeRange (pos.getStart(), pos.getEnd()), nullptr);

        if (midi == nullptr)
            return -1;

        midi->setName (name);

        for (auto& n : notes)
        {
            const auto b1 = midi->getContentBeatAtTime (te::TimePosition::fromSeconds (clipStart + n.start));
            const auto b2 = midi->getContentBeatAtTime (te::TimePosition::fromSeconds (clipStart + n.start + n.length));
            midi->getSequence().addNote (n.pitch, b1, b2 - b1, n.velocity, 0, &session.getEdit().getUndoManager());
        }

        session.save();
        return (int) notes.size();
    }
}
