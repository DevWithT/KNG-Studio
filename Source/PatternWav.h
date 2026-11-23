#pragma once

#include "EditSession.h"

// Renders one pattern block to a WAV loop in the song's Loops folder.
// Only the sounds that play in the block are rendered (plus the buses they feed), without the master chain.
namespace KngPatternWav
{
    inline juce::File getLoopsFolder (EditSession& session)
    {
        auto folder = session.getProjectFile().getParentDirectory().getChildFile ("Loops");
        folder.createDirectory();
        return folder;
    }

    // onDone gets the new file and an error message (empty when it worked)
    inline void render (EditSession& session, const EditSession::PatternBlock& block,
                        std::function<void (const juce::File&, const juce::String&)> onDone)
    {
        namespace te = tracktion;

        auto fail = [onDone] (const juce::String& message)
        {
            juce::MessageManager::callAsync ([onDone, message] { if (onDone) onDone ({}, message); });
        };

        auto& edit = session.getEdit();

        // The sounds this block plays
        juce::Array<te::Track*> tracks;

        for (auto* c : session.getBlockClips (block))
            if (auto* t = c->getTrack())
                tracks.addIfNotAlreadyThere (t);

        if (tracks.isEmpty())
        {
            fail ("This block has no sounds in it.");
            return;
        }

        // Follow each sound to the track its output goes to (the list grows as we go)
        for (int i = 0; i < tracks.size(); ++i)
            if (auto* at = dynamic_cast<te::AudioTrack*> (tracks[i]))
                if (auto* dest = at->getOutput().getDestinationTrack())
                    tracks.addIfNotAlreadyThere (dest);

        // Reverb and delay buses, so the sends are heard
        for (int i = 0; i < session.getNumTracks(); ++i)
            if (session.isReverbBus (i))
                if (auto* t = session.getTrack (i))
                    tracks.addIfNotAlreadyThere (t);

        session.getTransport().stop (false, false);

        auto safeName = juce::File::createLegalFileName (session.getPatternName (block.pattern).trim());

        if (safeName.isEmpty())
            safeName = "Loop";

        const auto dest = getLoopsFolder (session).getNonexistentChildFile (safeName, ".wav", false);

        auto format = std::make_shared<juce::WavAudioFormat>();

        te::Renderer::Parameters p (edit);
        p.destFile = dest;
        p.audioFormat = format.get();
        p.bitDepth = 24;
        p.blockSizeForAudio = 512;
        p.time = te::TimeRange (te::TimePosition::fromSeconds (block.start), te::TimePosition::fromSeconds (block.end));
        p.usePlugins = true;
        p.useMasterPlugins = false;
        p.canRenderInMono = false;
        p.checkNodesForAudio = false;
        p.tracksToDo = te::toBitSet (tracks);

        // Match the interface's sample rate, or fall back to 48k
        p.sampleRateForAudio = 48000.0;

        if (auto* device = edit.engine.getDeviceManager().deviceManager.getCurrentAudioDevice())
            p.sampleRateForAudio = device->getCurrentSampleRate();

        // Keep the render handle alive until it finishes
        auto holder = std::make_shared<std::shared_ptr<te::EditRenderer::Handle>>();

        *holder = te::EditRenderer::render (p, [holder, format, dest, onDone] (tl::expected<juce::File, std::string> result)
        {
            const juce::String error = result.has_value() ? juce::String() : juce::String (result.error());

            juce::MessageManager::callAsync ([holder, dest, onDone, error]
            {
                holder->reset();

                if (onDone)
                    onDone (dest, error);
            });
        });
    }
}
