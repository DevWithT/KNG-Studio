#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
juce::File EditSession::getLameExecutable()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("KNG Studio")
               .getChildFile ("lame.exe");
}

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

te::Renderer::Parameters EditSession::makeRenderParams (const juce::File& dest, bool loopOnly, int normaliseMode,
                                                        juce::AudioFormat& format)
{
    te::Renderer::Parameters p (*edit);
    p.destFile = dest;
    p.audioFormat = &format;
    p.bitDepth = 24;
    p.blockSizeForAudio = 512;
    p.time = getExportRange (loopOnly);
    p.usePlugins = true;
    p.useMasterPlugins = true;
    p.canRenderInMono = false;

    // MP3: 16-bit in, 320k CBR out
    if (auto* lame = dynamic_cast<juce::LAMEEncoderAudioFormat*> (&format))
    {
        p.bitDepth = 16;
        const auto options = lame->getQualityOptions();

        for (int i = 0; i < options.size(); ++i)
        {
            if (options[i].contains ("320"))
            {
                p.quality = i;
                break;
            }
        }
    }

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
                                                                  bool asMp3, std::function<void (const juce::String&)> onDone)
{
    getTransport().stop (false, false);

    // The format has to live until the render finishes, so the callback holds onto it
    std::shared_ptr<juce::AudioFormat> format;

    if (asMp3)
        format = std::make_shared<juce::LAMEEncoderAudioFormat> (getLameExecutable());
    else
        format = std::make_shared<juce::WavAudioFormat>();

    auto p = makeRenderParams (dest, loopOnly, normaliseMode, *format);

    // Let reverb and delay tails ring out on full-song exports
    if (! loopOnly)
        p.endAllowance = te::TimeDuration::fromSeconds (4.0);

    return te::EditRenderer::render (p, [onDone, format] (tl::expected<juce::File, std::string> result)
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

    auto format = std::make_shared<juce::WavAudioFormat>();

    // Stems are never normalised so their balance stays intact
    auto p = makeRenderParams (dest, loopOnly, 0, *format);
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

    return te::EditRenderer::render (p, [onDone, format] (tl::expected<juce::File, std::string> result)
    {
        const juce::String error = result.has_value() ? juce::String() : juce::String (result.error());

        juce::MessageManager::callAsync ([onDone, error]
        {
            if (onDone)
                onDone (error);
        });
    });
}

std::shared_ptr<te::EditRenderer::Handle> EditSession::exportTracks (const juce::Array<int>& trackIndices, const juce::File& dest,
                                                                     bool loopOnly, std::function<void (const juce::String&)> onDone)
{
    getTransport().stop (false, false);

    juce::Array<te::Track*> tracks;
    bool includesBus = false;

    for (int i : trackIndices)
    {
        if (auto* t = getTrack (i))
        {
            tracks.add (t);
            includesBus = includesBus || isReverbBus (i);
        }
    }

    if (tracks.isEmpty())
    {
        juce::MessageManager::callAsync ([onDone] { if (onDone) onDone ("No tracks to export"); });
        return {};
    }

    auto format = std::make_shared<juce::WavAudioFormat>();

    // Stems are never normalised so their balance stays intact
    auto p = makeRenderParams (dest, loopOnly, 0, *format);
    p.useMasterPlugins = false;
    p.checkNodesForAudio = false;
    p.tracksToDo = te::toBitSet (tracks);

    // A bus is silent on its own, so let the other tracks feed it without being heard
    if (includesBus)
        for (int i = 0; i < getNumTracks(); ++i)
            if (! trackIndices.contains (i))
                if (auto* other = getTrack (i))
                    p.tracksToProcessWhileMuted.add (other->itemID);

    if (! loopOnly)
        p.endAllowance = te::TimeDuration::fromSeconds (4.0);

    return te::EditRenderer::render (p, [onDone, format] (tl::expected<juce::File, std::string> result)
    {
        const juce::String error = result.has_value() ? juce::String() : juce::String (result.error());

        juce::MessageManager::callAsync ([onDone, error]
        {
            if (onDone)
                onDone (error);
        });
    });
}
