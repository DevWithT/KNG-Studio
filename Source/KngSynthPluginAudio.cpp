#include "KngSynthPlugin.h"
#include "KngSynthPresets.h"
#include "KngSynthView.h"

namespace te = tracktion;

//==============================================================================
// Sound

void KngSynthPlugin::render (juce::AudioBuffer<float>& out, int start, int num, const Block& b)
{
    using namespace KngSynth;

    const float sr = (float) sampleRate;

    if (num <= 0 || sr <= 0.0f)
        return;

    auto* left  = out.getWritePointer (0, start);
    auto* right = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

    // Settings for this block
    const int waveA = juce::roundToInt (b.v[oscAWave]);
    const int waveB = juce::roundToInt (b.v[oscBWave]);
    const int uniA  = juce::jlimit (1, maxUnison, juce::roundToInt (b.v[oscAUnison]));
    const int uniB  = juce::jlimit (1, maxUnison, juce::roundToInt (b.v[oscBUnison]));
    const double mulA = std::exp2 ((b.v[oscAOct] * 12.0 + b.v[oscASemi] + b.v[oscAFine] / 100.0) / 12.0);
    const double mulB = std::exp2 ((b.v[oscBOct] * 12.0 + b.v[oscBSemi] + b.v[oscBFine] / 100.0) / 12.0);
    const float lvlA = b.v[oscALevel], lvlB = b.v[oscBLevel];
    const float lvlSub = b.v[subLevel], lvlNoise = b.v[noiseLevel];
    const float width = b.v[spread];

    // Unison: spread the copies in pitch (up to +-50 cents) and across the stereo field
    std::array<double, maxUnison> ratioA {}, ratioB {};
    std::array<float, maxUnison> panLA {}, panRA {}, panLB {}, panRB {};

    auto setupUnison = [width] (int n, float detune, std::array<double, maxUnison>& ratio,
                                std::array<float, maxUnison>& pl, std::array<float, maxUnison>& pr)
    {
        const float norm = 1.0f / std::sqrt ((float) n);

        for (int u = 0; u < n; ++u)
        {
            const float pos = n > 1 ? (float) u / (float) (n - 1) * 2.0f - 1.0f : 0.0f;
            ratio[(size_t) u] = std::exp2 (pos * detune * 0.5 / 12.0);

            const float pan = pos * width;
            pl[(size_t) u] = norm * std::sqrt (0.5f * (1.0f - pan));
            pr[(size_t) u] = norm * std::sqrt (0.5f * (1.0f + pan));
        }
    };

    setupUnison (uniA, b.v[oscADetune], ratioA, panLA, panRA);
    setupUnison (uniB, b.v[oscBDetune], ratioB, panLB, panRB);

    const int type = juce::roundToInt (b.v[filterType]);
    const float baseCut = b.v[cutoff];
    const float k = 2.0f - 1.96f * b.v[reso];
    const float driveGain = 1.0f + b.v[drive] * 6.0f;
    const float driveNorm = 1.0f / std::sqrt (driveGain);
    const float envOctaves = b.v[filterEnv] * 6.0f;
    const float keyAmount = b.v[keyTrack];

    const float fa = b.v[fenvA], fd = b.v[fenvD], fs = b.v[fenvS], fr = b.v[fenvR];
    const float aa = b.v[ampA],  ad = b.v[ampD],  as = b.v[ampS],  ar = b.v[ampR];

    const int lfoWave = juce::roundToInt (b.v[lfoShape]);
    const double lfoInc = b.v[lfoRate] / sampleRate;
    const float vibrato = b.v[lfoPitch] * 2.0f;     // up to 2 semitones
    const float wobble  = b.v[lfoCutoff] * 4.0f;    // up to 4 octaves

    const double glideTime = b.v[glide];
    const double glideCoef = glideTime > 0.0005 ? 1.0 - std::exp (-1.0 / (glideTime * sampleRate)) : 1.0;
    const float velAmount = b.v[velSens];
    const float gain = juce::Decibels::decibelsToGain (b.v[master]);
    const float maxCut = sr * 0.45f;
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    for (int i = 0; i < num; ++i)
    {
        // One LFO shared by every voice
        lfoPhase += lfoInc;

        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;

        const float lfo = lfoWave == 0 ? (float) std::sin (lfoPhase * twoPi)
                        : lfoWave == 1 ? (float) (1.0 - 4.0 * std::abs (lfoPhase - 0.5))
                        : lfoWave == 2 ? (float) (2.0 * lfoPhase - 1.0)
                                       : (lfoPhase < 0.5 ? 1.0f : -1.0f);

        float mixL = 0.0f, mixR = 0.0f;

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            const float ampEnv  = v.amp.process (aa, ad, as, ar, sr);
            const float filtEnv = v.filt.process (fa, fd, fs, fr, sr);

            if (! v.amp.isActive())
            {
                v.active = false;
                continue;
            }

            v.pitch += (v.targetPitch - v.pitch) * glideCoef;
            const double baseInc = 440.0 * std::exp2 ((v.pitch + lfo * vibrato - 69.0) / 12.0) / sampleRate;

            float sL = 0.0f, sR = 0.0f;

            if (lvlA > 0.0f)
            {
                const double inc = baseInc * mulA;

                for (int u = 0; u < uniA; ++u)
                {
                    const double dt = inc * ratioA[(size_t) u];
                    auto& ph = v.phaseA[(size_t) u];
                    const float s = oscSample (waveA, ph, dt) * lvlA;
                    sL += s * panLA[(size_t) u];
                    sR += s * panRA[(size_t) u];
                    ph += dt;

                    if (ph >= 1.0)
                        ph -= 1.0;
                }
            }

            if (lvlB > 0.0f)
            {
                const double inc = baseInc * mulB;

                for (int u = 0; u < uniB; ++u)
                {
                    const double dt = inc * ratioB[(size_t) u];
                    auto& ph = v.phaseB[(size_t) u];
                    const float s = oscSample (waveB, ph, dt) * lvlB;
                    sL += s * panLB[(size_t) u];
                    sR += s * panRB[(size_t) u];
                    ph += dt;

                    if (ph >= 1.0)
                        ph -= 1.0;
                }
            }

            // Sub: a sine one octave under oscillator A
            if (lvlSub > 0.0f)
            {
                const float s = (float) std::sin (v.subPhase * twoPi) * lvlSub * 0.7f;
                sL += s;
                sR += s;
                v.subPhase += baseInc * mulA * 0.5;

                if (v.subPhase >= 1.0)
                    v.subPhase -= 1.0;
            }

            if (lvlNoise > 0.0f)
            {
                const float s = (v.random.nextFloat() * 2.0f - 1.0f) * lvlNoise * 0.4f;
                sL += s;
                sR += s;
            }

            // Filter, moved by its envelope, the LFO and the note played
            const float octaves = envOctaves * filtEnv + wobble * lfo + keyAmount * (float) (v.pitch - 60.0) / 12.0f;
            const float fc = juce::jlimit (20.0f, maxCut, baseCut * std::exp2 (octaves));
            const float g = std::tan (juce::MathConstants<float>::pi * fc / sr);

            const float yL = v.filterL.process (std::tanh (sL * driveGain) * driveNorm, g, k, type);
            const float yR = v.filterR.process (std::tanh (sR * driveGain) * driveNorm, g, k, type);

            const float level = ampEnv * ((1.0f - velAmount) + velAmount * v.velocity);
            mixL += yL * level;
            mixR += yR * level;
        }

        left[i] += mixL * gain;

        if (right != nullptr)
            right[i] += mixR * gain;
    }
}

void KngSynthPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    juce::ScopedNoDenormals noDenormals;
    auto& out = *fc.destBuffer;

    Block b;

    for (int i = 0; i < KngSynth::numParams; ++i)
        b.v[i] = params[(size_t) i]->getCurrentValue();

    for (int ch = 0; ch < out.getNumChannels(); ++ch)
        out.clear (ch, fc.bufferStartSample, fc.bufferNumSamples);

    if (wasPlaying && ! fc.isPlaying)
    {
        stopFadeTotal = juce::jmax (1, (int) (0.005 * sampleRate));
        stopFadeLeft = stopFadeTotal;
    }

    wasPlaying = fc.isPlaying;

    int cursor = 0;

    auto renderUpTo = [&] (int end)
    {
        if (end > cursor)
            render (out, fc.bufferStartSample + cursor, end - cursor, b);

        cursor = juce::jmax (cursor, end);
    };

    if (fc.bufferForMidiMessages != nullptr)
    {
        fc.bufferForMidiMessages->sortByTimestamp();

        for (auto& m : *fc.bufferForMidiMessages)
        {
            const int pos = juce::jlimit (0, fc.bufferNumSamples,
                                          (int) ((m.getTimeStamp() + fc.midiBufferOffset) * sampleRate));
            renderUpTo (pos);

            if (m.isNoteOn())
            {
                noteOn (m.getNoteNumber(), m.getFloatVelocity(), b);
            }
            else if (m.isNoteOff())
            {
                noteOff (m.getNoteNumber(), b);
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff())
            {
                for (auto& v : voices)
                {
                    v.amp.noteOff();
                    v.filt.noteOff();
                }

                heldCount = 0;
            }
        }
    }

    renderUpTo (fc.bufferNumSamples);

    // Fading out after Stop
    if (stopFadeLeft > 0)
    {
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float g = (float) stopFadeLeft / (float) stopFadeTotal;

            for (int ch = 0; ch < out.getNumChannels(); ++ch)
                out.getWritePointer (ch, fc.bufferStartSample)[i] *= g;

            if (stopFadeLeft > 0)
                --stopFadeLeft;
        }

        if (stopFadeLeft == 0)
            reset();
    }
}
