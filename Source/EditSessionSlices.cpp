#include "EditSession.h"
#include "SlicePitchShift.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace te = tracktion;

//==============================================================================
// Slices: find the hits in a clip, write each slice to its own file, and send
// the slices to Rack rows or to Drum Kit pads.

juce::File EditSession::writeSliceFile (te::AudioClipBase& clip, double fromTime, double toTime, int number)
{
    const auto source = clip.getAudioFile().getFile();

    if (! source.existsAsFile())
        return {};

    const auto pos = clip.getPosition();
    const double clipStart = pos.getStart().inSeconds();
    const double offset = pos.getOffset().inSeconds();

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));

    if (reader == nullptr)
        return {};

    const auto startSample = (juce::int64) ((offset + fromTime - clipStart) * reader->sampleRate);
    const int len = (int) ((toTime - fromTime) * reader->sampleRate);

    if (len <= 0)
        return {};

    juce::AudioBuffer<float> buffer ((int) reader->numChannels, len);
    reader->read (&buffer, 0, len, startSample, true, true);

    auto folder = getProjectFolder().getChildFile ("Audio");
    folder.createDirectory();

    const auto baseName = juce::File::createLegalFileName (clip.getName().isNotEmpty() ? clip.getName()
                                                                                       : source.getFileNameWithoutExtension());
    auto file = folder.getNonexistentChildFile (baseName + " slice " + juce::String (number), ".wav");
    auto* stream = new juce::FileOutputStream (file);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream, reader->sampleRate,
                                                                          (unsigned int) buffer.getNumChannels(),
                                                                          24, {}, 0));

    if (writer == nullptr)
    {
        delete stream;
        return {};
    }

    const int fxIndex = number - 1;

    if (fxIndex >= 0 && fxIndex < (int) sliceFxForNextWrite.size() && ! sliceFxForNextWrite[(size_t) fxIndex].isDefault())
        buffer = KngSlicePitchShift::applyFx (buffer, reader->sampleRate,
                                              sliceFxForNextWrite[(size_t) fxIndex],
                                              sliceKeepLengthForNextWrite);

    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    writer.reset();
    return file;
}

//==============================================================================
std::vector<double> EditSession::findHits (te::AudioClipBase& clip, float sensitivity)
{
    std::vector<double> result;
    const auto source = clip.getAudioFile().getFile();

    if (! source.existsAsFile())
        return result;

    const auto pos = clip.getPosition();
    const double length = pos.getLength().inSeconds();
    const double offset = pos.getOffset().inSeconds();

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));

    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->numChannels == 0)
        return result;

    // Loudness every 5 ms, measured over 10 ms
    const int hop = juce::jmax (1, (int) (reader->sampleRate * 0.005));
    const int numHops = (int) (length / 0.005);

    if (numHops < 12)
        return result;

    const auto firstSample = (juce::int64) (offset * reader->sampleRate);
    const int chunkHops = 64;
    std::vector<float> meanSquare ((size_t) numHops, 0.0f);
    juce::AudioBuffer<float> buffer ((int) reader->numChannels, chunkHops * hop);

    for (int h0 = 0; h0 < numHops; h0 += chunkHops)
    {
        const int n = juce::jmin (chunkHops, numHops - h0);
        buffer.clear();
        reader->read (&buffer, 0, n * hop, firstSample + (juce::int64) h0 * hop, true, true);

        for (int h = 0; h < n; ++h)
        {
            double sum = 0.0;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                const float* d = buffer.getReadPointer (ch, h * hop);

                for (int i = 0; i < hop; ++i)
                    sum += (double) d[i] * (double) d[i];
            }

            meanSquare[(size_t) (h0 + h)] = (float) (sum / (double) (hop * buffer.getNumChannels()));
        }
    }

    std::vector<float> env ((size_t) numHops, 0.0f);
    float peak = 0.0f;

    for (int h = 0; h < numHops; ++h)
    {
        const float prev = h > 0 ? meanSquare[(size_t) h - 1] : meanSquare[(size_t) h];
        env[(size_t) h] = std::sqrt (0.5f * (meanSquare[(size_t) h] + prev));
        peak = juce::jmax (peak, env[(size_t) h]);
    }

    if (peak < 1.0e-4f)
        return result;

    const float s = juce::jlimit (0.0f, 1.0f, sensitivity);
    const float floorLevel = peak * (0.05f - 0.04f * s);
    const float threshold = 2.2f - 1.6f * s;

    // How fast the loudness climbs over the last 15 ms
    auto rise = [&env] (int i)
    {
        if (i < 3 || i >= (int) env.size())
            return 0.0f;

        return std::log (env[(size_t) i] + 1.0e-5f) - std::log (env[(size_t) i - 3] + 1.0e-5f);
    };

    struct Hit { int hop; float strength; double time; };
    std::vector<Hit> hits;

    for (int h = 3; h < numHops - 1; ++h)
    {
        const float d = rise (h);

        if (d < threshold || d < rise (h - 1) || d < rise (h + 1) || env[(size_t) h] < floorLevel)
            continue;

        // Walk back to where the climb began
        int start = h;

        while (start > 0 && h - start < 8 && env[(size_t) start - 1] < env[(size_t) start])
            --start;

        const double t = juce::jmax (0.0, (double) start * 0.005 - 0.003);

        if (t < 0.03 || t > length - 0.03)
            continue;

        // Hits closer than 60 ms count as one, the stronger wins
        if (! hits.empty() && h - hits.back().hop < 12)
        {
            if (d > hits.back().strength)
                hits.back() = { h, d, t };

            continue;
        }

        hits.push_back ({ h, d, t });
    }

    // 16 slices at most, so keep the 15 strongest cuts
    if (hits.size() > 15)
    {
        std::sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.strength > b.strength; });
        hits.erase (hits.begin() + 15, hits.end());
        std::sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.time < b.time; });
    }

    for (auto& hit : hits)
        result.push_back (hit.time);

    return result;
}

//==============================================================================
int EditSession::chopClipAtCuts (te::AudioClipBase& clip, const std::vector<double>& cuts)
{
    if (getTransport().isRecording() || cuts.size() < 2)
        return -1;

    const auto pos = clip.getPosition();
    const int numSlices = juce::jmin (16, (int) cuts.size() - 1);
    int firstIndex = -1;

    // Tempo match: squeeze or stretch the chop timing from the clip's tempo to the song's tempo
    const double clipStartSec = pos.getStart().inSeconds();
    const double projectBpm = getBpmAt (clipStartSec);
    const double tempoScale = (sliceSourceBpmForNextChop > 0.0 && projectBpm > 0.0) ? sliceSourceBpmForNextChop / projectBpm : 1.0;
    auto scaledTime = [clipStartSec, tempoScale] (double t)
    {
        return te::TimePosition::fromSeconds (clipStartSec + (t - clipStartSec) * tempoScale);
    };

    for (int i = 0; i < numSlices; ++i)
    {
        const auto file = writeSliceFile (clip, cuts[(size_t) i], cuts[(size_t) i + 1], i + 1);

        if (! file.existsAsFile())
            continue;

        // One Rack row per slice
        const int index = addSampleChannel (file);

        if (index < 0)
            continue;

        if (firstIndex < 0)
            firstIndex = index;

        // A hit where this slice was, so the chops play back like the original
        if (auto* track = getTrack (index))
        {
            if (auto midi = track->insertMIDIClip (te::TimeRange (pos.getStart(), scaledTime (pos.getEnd().inSeconds())), nullptr))
            {
                midi->setName ("Chops");
                const auto b1 = midi->getContentBeatAtTime (scaledTime (cuts[(size_t) i]));
                const auto b2 = midi->getContentBeatAtTime (scaledTime (cuts[(size_t) i + 1]));
                midi->getSequence().addNote (60, b1, b2 - b1, 100, 0, &edit->getUndoManager());
            }
        }
    }

    if (firstIndex < 0)
        return -1;

    // So you don't hear it twice
    beginEdit();
    clip.setMuted (true);
    save();

    return firstIndex;
}

//==============================================================================
int EditSession::slicesToPads (te::AudioClipBase& clip, const std::vector<double>& cuts, int trackIndex)
{
    if (getTransport().isRecording() || cuts.size() < 2 || trackIndex < 0)
        return -1;

    const int numSlices = juce::jmin (16, (int) cuts.size() - 1);
    int loaded = 0;

    for (int i = 0; i < numSlices; ++i)
    {
        const auto file = writeSliceFile (clip, cuts[(size_t) i], cuts[(size_t) i + 1], i + 1);

        if (file.existsAsFile() && setPadSample (trackIndex, i, file))
            ++loaded;
    }

    return loaded;
}
