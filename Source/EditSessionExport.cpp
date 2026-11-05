#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
juce::File EditSession::getExportsFolder()
{
    auto folder = getProjectFolder().getChildFile ("Exports");
    folder.createDirectory();
    return folder;
}

bool EditSession::trackHasContent (int trackIndex)
{
    if (auto* t = getTrack (trackIndex))
        return ! t->getClips().isEmpty() || isReverbBus (trackIndex);

    return false;
}

te::TimeRange EditSession::getExportRange (bool loopOnly)
{
    if (loopOnly && getLoopEnd() > getLoopStart())
        return getTransport().getLoopRange();

    return te::TimeRange (te::TimePosition(), te::TimePosition() + edit->getLength());
}

te::Renderer::Parameters EditSession::makeRenderParams (const juce::File& dest, bool loopOnly, int normaliseMode)
{
    static juce::WavAudioFormat wavFormat;

    te::Renderer::Parameters p (*edit);
    p.destFile = dest;
    p.audioFormat = &wavFormat;
    p.bitDepth = 24;
    p.blockSizeForAudio = 512;
    p.time = getExportRange (loopOnly);
    p.usePlugins = true;
    p.useMasterPlugins = true;
    p.canRenderInMono = false;

    // Match the interface's sample rate, or fall back to 48k
    p.sampleRateForAudio = 48000.0;

    if (auto* device = engine.getDeviceManager().deviceManager.getCurrentAudioDevice())
        p.sampleRateForAudio = device->getCurrentSampleRate();

    if (normaliseMode == 1)
    {
        p.shouldNormalise = true;
        p.normaliseToLevelDb = -1.0f;
    }
    else if (normaliseMode == 2)
    {
        p.shouldNormaliseByLUFS = true;
        p.normaliseToLevelDb = -14.0f;
        p.limitTruePeak = true;
        p.truePeakCeilingDb = -1.0f;
    }

    return p;
}

//==============================================================================
std::shared_ptr<te::EditRenderer::Handle> EditSession::exportMix (const juce::File& dest, bool loopOnly, int normaliseMode,
                                                                  std::function<void (const juce::String&)> onDone)
{
    getTransport().stop (false, false);

    auto p = makeRenderParams (dest, loopOnly, normaliseMode);

    // Let reverb and delay tails ring out on full-song exports
    if (! loopOnly)
        p.endAllowance = te::TimeDuration::fromSeconds (4.0);

    return te::EditRenderer::render (p, [onDone] (tl::expected<juce::File, std::string> result)
    {
        const juce::String error = result.has_value() ? juce::String() : juce::String (result.error());

        juce::MessageManager::callAsync ([onDone, error]
        {
            if (onDone)
                onDone (error);
        });
    });
}

std::shared_ptr<te::EditRenderer::Handle> EditSession::exportStem (int trackIndex, const juce::File& dest, bool loopOnly,
                                                                   std::function<void (const juce::String&)> onDone)
{
    getTransport().stop (false, false);

    auto* track = getTrack (trackIndex);

    if (track == nullptr)
    {
        juce::MessageManager::callAsync ([onDone] { if (onDone) onDone ("Track not found"); });
        return {};
    }

    // Stems are never normalised so their balance stays intact
    auto p = makeRenderParams (dest, loopOnly, 0);
    p.useMasterPlugins = false;
    p.checkNodesForAudio = false;

    juce::Array<te::Track*> justThisTrack;
    justThisTrack.add (track);
    p.tracksToDo = te::toBitSet (justThisTrack);

    // The reverb bus is silent on its own, so let the other tracks feed it without being heard
    if (isReverbBus (trackIndex))
        for (int i = 0; i < getNumTracks(); ++i)
            if (i != trackIndex)
                if (auto* other = getTrack (i))
                    p.tracksToProcessWhileMuted.add (other->itemID);

    if (! loopOnly)
        p.endAllowance = te::TimeDuration::fromSeconds (4.0);

    return te::EditRenderer::render (p, [onDone] (tl::expected<juce::File, std::string> result)
    {
        const juce::String error = result.has_value() ? juce::String() : juce::String (result.error());

        juce::MessageManager::callAsync ([onDone, error]
        {
            if (onDone)
                onDone (error);
        });
    });
}
