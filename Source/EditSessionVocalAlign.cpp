#include "EditSession.h"
#include <algorithm>

namespace te = tracktion;

namespace
{
    constexpr double frameSec = 0.01;   // loudness is measured every 10 ms

    // How loud the audio is every 10 ms between from and to (timeline seconds), across every clip in the list
    std::vector<float> loudnessOf (const juce::Array<te::WaveAudioClip*>& clips, double from, double to)
    {
        const int frames = juce::jmax (0, (int) std::ceil ((to - from) / frameSec));
        std::vector<float> env ((size_t) frames, 0.0f);

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        for (auto* clip : clips)
        {
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (clip->getPlaybackFile().getFile()));

            if (reader == nullptr || reader->sampleRate <= 0.0)
                continue;

            const double sr = reader->sampleRate;
            const auto pos = clip->getPosition();
            const double clipStart = pos.getStart().inSeconds();
            const double offset = pos.getOffset().inSeconds();
            const double a = juce::jmax (from, clipStart);
            const double b = juce::jmin (to, pos.getEnd().inSeconds());

            if (b <= a)
                continue;

            const int hop = juce::jmax (1, (int) (frameSec * sr));
            const int chans = juce::jlimit (1, 2, (int) reader->numChannels);
            juce::AudioBuffer<float> buf (chans, hop);

            for (int k = (int) ((a - from) / frameSec); k < frames; ++k)
            {
                const double t = from + k * frameSec;

                if (t >= b)
                    break;

                const auto src = (juce::int64) ((offset + (t - clipStart)) * sr);

                if (src < 0 || src + hop > reader->lengthInSamples)
                    continue;

                reader->read (&buf, 0, hop, src, true, chans > 1);

                double sum = 0.0;

                for (int ch = 0; ch < chans; ++ch)
                {
                    const float* d = buf.getReadPointer (ch);

                    for (int i = 0; i < hop; ++i)
                        sum += d[i] * d[i];
                }

                env[(size_t) k] = juce::jmax (env[(size_t) k], (float) std::sqrt (sum / (hop * chans)));
            }
        }

        return env;
    }

    // Turns loudness into "where words start", which lines up far better than plain loudness
    std::vector<float> onsetsOf (const std::vector<float>& env)
    {
        std::vector<float> out (env.size(), 0.0f);

        for (size_t k = 1; k < env.size(); ++k)
        {
            const float now = std::sqrt (env[k]), before = std::sqrt (env[k - 1]);
            out[k] = juce::jmax (0.0f, now - before) * 2.0f + 0.3f * now;
        }

        return out;
    }

    // How far (in frames) part of the double should move to best match the lead, searching around a starting guess
    int bestLag (const std::vector<float>& lead, const std::vector<float>& dub, int start, int length, int centre, int range)
    {
        double best = -1.0e9;
        int bestL = centre;

        for (int lag = centre - range; lag <= centre + range; ++lag)
        {
            double sum = 0.0, dd = 0.0, ll = 0.0;

            for (int i = start; i < start + length; ++i)
            {
                const int j = i + lag;

                if (i < 0 || i >= (int) dub.size() || j < 0 || j >= (int) lead.size())
                    continue;

                sum += dub[(size_t) i] * lead[(size_t) j];
                dd  += dub[(size_t) i] * dub[(size_t) i];
                ll  += lead[(size_t) j] * lead[(size_t) j];
            }

            if (dd <= 0.0 || ll <= 0.0)
                continue;

            // A small nudge toward smaller moves, so silence never drags a phrase away
            const double score = sum / std::sqrt (dd * ll) - 0.0008 * std::abs (lag - centre);

            if (score > best)
            {
                best = score;
                bestL = lag;
            }
        }

        return bestL;
    }

    struct Phrase { int a, b; };
}

//==============================================================================
int EditSession::alignClipToTrack (te::WaveAudioClip& dub, int leadTrackIndex)
{
    auto* leadTrack = getTrack (leadTrackIndex);
    auto* dubTrack = dub.getClipTrack();

    if (leadTrack == nullptr || dubTrack == nullptr || getTransport().isRecording())
        return -1;

    juce::Array<te::WaveAudioClip*> leadClips;

    for (auto* c : leadTrack->getClips())
        if (auto* w = dynamic_cast<te::WaveAudioClip*> (c))
            if (w != &dub)
                leadClips.add (w);

    if (leadClips.isEmpty())
        return -1;

    juce::MouseCursor::showWaitCursor();

    // Look a little past both ends of the double
    const double dubStart = dub.getPosition().getStart().inSeconds();
    const double dubEnd = dub.getPosition().getEnd().inSeconds();
    const double from = juce::jmax (0.0, dubStart - 0.5);
    const double to = dubEnd + 0.5;

    juce::Array<te::WaveAudioClip*> dubOnly;
    dubOnly.add (&dub);

    const auto dubLoud = loudnessOf (dubOnly, from, to);
    const auto lead = onsetsOf (loudnessOf (leadClips, from, to));
    const auto dubOn = onsetsOf (dubLoud);

    const int i0 = (int) ((dubStart - from) / frameSec);
    const int i1 = juce::jmin ((int) dubLoud.size(), (int) ((dubEnd - from) / frameSec));

    if (i1 - i0 < 10)
    {
        juce::MouseCursor::hideWaitCursor();
        return -1;
    }

    // 1. Line up: slide the whole double (up to 0.4 s either way)
    const int globalLag = bestLag (lead, dubOn, i0, i1 - i0, 0, 40);

    // 2. Tighten: find the phrases in the double (sound with 120 ms or more of quiet between)
    float peak = 0.0f;

    for (int i = i0; i < i1; ++i)
        peak = juce::jmax (peak, dubLoud[(size_t) i]);

    const float threshold = juce::jmax (0.002f, peak * 0.06f);
    constexpr int minGap = 12, minPhrase = 8;
    std::vector<Phrase> phrases;
    bool inPhrase = false;
    int phraseStart = 0, quiet = 0;

    for (int i = i0; i < i1; ++i)
    {
        if (dubLoud[(size_t) i] > threshold)
        {
            if (! inPhrase)
            {
                inPhrase = true;
                phraseStart = i;
            }

            quiet = 0;
        }
        else if (inPhrase && ++quiet >= minGap)
        {
            if (i - quiet + 1 - phraseStart >= minPhrase)
                phrases.push_back ({ phraseStart, i - quiet + 1 });

            inPhrase = false;
            quiet = 0;
        }
    }

    if (inPhrase && i1 - phraseStart >= minPhrase)
        phrases.push_back ({ phraseStart, i1 });

    // Each phrase's own nudge on top of the whole-clip move, never far enough to overlap its neighbours
    std::vector<int> nudge (phrases.size(), 0);

    for (size_t k = 0; k < phrases.size(); ++k)
    {
        const auto& p = phrases[k];
        const int gapBefore = k == 0 ? 100 : p.a - phrases[k - 1].b;
        const int gapAfter = k + 1 == phrases.size() ? 100 : phrases[k + 1].a - p.b;
        const int room = juce::jmin (15, gapBefore / 2 - 1, gapAfter / 2 - 1);

        if (room <= 0)
            continue;

        // Match on the start of the phrase, where timing is heard most
        const int lag = bestLag (lead, dubOn, p.a - 3, juce::jmin (p.b - p.a, 40) + 3, globalLag, room);
        nudge[k] = lag - globalLag;
    }

    const bool anyNudge = std::any_of (nudge.begin(), nudge.end(), [] (int n) { return std::abs (n) >= 2; });

    if (std::abs (globalLag) < 1 && ! anyNudge)
    {
        juce::MouseCursor::hideWaitCursor();
        return 0;
    }

    clearSelection();
    beginEdit();
    int moves = 0;

    const double shift = globalLag * frameSec;

    if (std::abs (globalLag) >= 1)
    {
        dub.setStart (dub.getPosition().getStart() + te::TimeDuration::fromSeconds (shift), false, true);
        ++moves;
    }

    if (anyNudge && phrases.size() > 1)
    {
        // Cut in the middle of each quiet gap, so every piece holds one phrase
        std::vector<te::Clip*> pieces { &dub };
        te::Clip* current = &dub;

        for (size_t k = 1; k < phrases.size(); ++k)
        {
            const double cut = from + 0.5 * (phrases[k - 1].b + phrases[k].a) * frameSec + shift;
            auto* right = dubTrack->splitClip (*current, te::TimePosition::fromSeconds (cut));

            if (right == nullptr)
                break;

            pieces.push_back (right);
            current = right;
        }

        for (size_t k = 0; k < pieces.size() && k < nudge.size(); ++k)
        {
            auto* piece = pieces[k];

            if (std::abs (nudge[k]) >= 2)
            {
                piece->setStart (piece->getPosition().getStart() + te::TimeDuration::fromSeconds (nudge[k] * frameSec), false, true);
                ++moves;
            }

            // Tiny fades so the cuts never click
            if (auto* acb = dynamic_cast<te::AudioClipBase*> (piece))
            {
                acb->setFadeIn (te::TimeDuration::fromSeconds (0.005));
                acb->setFadeOut (te::TimeDuration::fromSeconds (0.005));
            }
        }
    }
    else if (anyNudge && phrases.size() == 1 && std::abs (nudge[0]) >= 2)
    {
        dub.setStart (dub.getPosition().getStart() + te::TimeDuration::fromSeconds (nudge[0] * frameSec), false, true);
        ++moves;
    }

    juce::MouseCursor::hideWaitCursor();
    save();
    return moves;
}
