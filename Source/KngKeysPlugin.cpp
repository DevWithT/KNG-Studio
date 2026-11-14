#include "KngKeysPlugin.h"
#include "KngKeysView.h"

namespace te = tracktion;

const char* KngKeysPlugin::xmlTypeName = "kngkeys";

static std::vector<KngParamDef> keysParams()
{
    return {
        { "sound",     "Sound",     0.0f,  2.0f,  0.0f, 0.0f, 1.0f, "",   "Piano|Rhodes|Organ" },
        { "tone",      "Tone",      0.0f,  1.0f,  0.6f, 0.0f, 0.0f, "%",  nullptr },
        { "character", "Character", 0.0f,  1.0f,  0.5f, 0.0f, 0.0f, "%",  nullptr },
        { "decay",     "Decay",     0.0f,  1.0f,  0.5f, 0.0f, 0.0f, "%",  nullptr },
        { "motion",    "Motion",    0.0f,  1.0f,  0.0f, 0.0f, 0.0f, "%",  nullptr },
        { "speed",     "Speed",     0.3f,  8.0f,  4.0f, 2.0f, 0.0f, "Hz", nullptr },
        { "width",     "Width",     0.0f,  1.0f,  0.5f, 0.0f, 0.0f, "%",  nullptr },
        { "velSens",   "Velocity",  0.0f,  1.0f,  0.7f, 0.0f, 0.0f, "%",  nullptr },
        { "level",     "Volume",  -24.0f,  6.0f, -6.0f, 0.0f, 0.0f, "dB", nullptr }
    };
}

static std::vector<KngPresetDef> keysPresets()
{
    using P = KngKeysPlugin;

    return {
        { "Default", {} },
        { "Ballad Piano",   { { P::sound, 0.0f }, { P::tone, 0.55f }, { P::character, 0.4f }, { P::decay, 0.75f }, { P::width, 0.7f } } },
        { "Bright Piano",   { { P::sound, 0.0f }, { P::tone, 0.85f }, { P::character, 0.8f }, { P::decay, 0.5f } } },
        { "Felt Piano",     { { P::sound, 0.0f }, { P::tone, 0.3f }, { P::character, 0.1f }, { P::decay, 0.6f }, { P::velSens, 0.4f } } },
        { "Classic Rhodes", { { P::sound, 1.0f }, { P::tone, 0.6f }, { P::character, 0.5f }, { P::decay, 0.6f }, { P::motion, 0.35f }, { P::speed, 4.5f } } },
        { "Lo-fi Rhodes",   { { P::sound, 1.0f }, { P::tone, 0.35f }, { P::character, 0.3f }, { P::decay, 0.5f }, { P::motion, 0.5f }, { P::speed, 3.0f } } },
        { "Barking Rhodes", { { P::sound, 1.0f }, { P::tone, 0.75f }, { P::character, 0.9f }, { P::decay, 0.5f }, { P::velSens, 0.9f } } },
        { "Gospel Organ",   { { P::sound, 2.0f }, { P::tone, 0.7f }, { P::character, 0.8f }, { P::motion, 0.6f }, { P::speed, 6.5f } } },
        { "Soft Organ",     { { P::sound, 2.0f }, { P::tone, 0.4f }, { P::character, 0.2f }, { P::motion, 0.3f }, { P::speed, 1.0f } } },
        { "Church Organ",   { { P::sound, 2.0f }, { P::tone, 0.6f }, { P::character, 1.0f }, { P::motion, 0.0f }, { P::width, 0.9f } } }
    };
}

KngKeysPlugin::KngKeysPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, keysParams(), keysPresets())
{
}

void KngKeysPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    reset();
}

void KngKeysPlugin::reset()
{
    for (auto& v : voices)
        v = {};

    lowState = {};
    litLow = 0;
    litHigh = 0;
}

bool KngKeysPlugin::isNoteLit (int note) const
{
    if (! juce::isPositiveAndBelow (note, 128))
        return false;

    const auto bits = note < 64 ? litLow.load() : litHigh.load();
    return ((bits >> (note & 63)) & 1) != 0;
}

void KngKeysPlugin::publishLitNotes()
{
    juce::uint64 low = 0, high = 0;

    for (auto& v : voices)
    {
        if (! v.active || ! v.held)
            continue;

        if (v.note < 64)
            low |= (juce::uint64) 1 << v.note;
        else
            high |= (juce::uint64) 1 << (v.note - 64);
    }

    litLow = low;
    litHigh = high;
}

//==============================================================================
void KngKeysPlugin::noteOn (int note, float velocity)
{
    // A free voice, or else the quietest one
    Voice* target = nullptr;

    for (auto& v : voices)
    {
        if (! v.active)
        {
            target = &v;
            break;
        }
    }

    if (target == nullptr)
    {
        target = &voices[0];

        for (auto& v : voices)
            if (v.rel * v.env[0] < target->rel * target->env[0])
                target = &v;
    }

    auto& v = *target;
    v = {};

    const double sr = sampleRate;
    const int s = juce::jlimit (0, 2, juce::roundToInt (get (sound)));
    const float vs = get (velSens);
    const float chr = get (character);
    const double f0 = 440.0 * std::exp2 ((note - 69) / 12.0);
    const double nyquist = sr * 0.45;

    v.active = true;
    v.held = true;
    v.note = note;
    v.sound = s;
    v.gain = (1.0f - vs) + vs * velocity;
    v.pan = juce::jlimit (-1.0f, 1.0f, (note - 60) / 30.0f) * get (width);

    auto decayMul = [sr] (double seconds) { return (float) std::exp (-1.0 / (juce::jmax (0.005, seconds) * sr)); };

    if (s == 0)
    {
        // Piano: slightly stretched partials, the high ones dying first; harder playing is brighter
        const double lengthS = (0.8 + get (decay) * 7.0) * juce::jlimit (0.35, 1.6, 1.6 - (note - 36) / 50.0);
        const float bright = juce::jlimit (0.0f, 1.0f, 0.25f + 0.5f * velocity * (0.4f + chr));

        for (int k = 0; k < numPartials; ++k)
        {
            const double n = k + 1;
            const double fk = n * f0 * std::sqrt (1.0 + 0.0004 * n * n);

            if (fk >= nyquist)
                break;

            v.inc[(size_t) k] = fk / sr;
            v.amp[(size_t) k] = (float) std::pow (n, -(1.7 - 1.1 * bright));
            v.env[(size_t) k] = 1.0f;
            v.mul[(size_t) k] = decayMul (lengthS / (1.0 + 0.45 * k));
            v.numUsed = k + 1;
        }

        v.click = 0.25f * chr * velocity;
        v.clickMul = decayMul (0.004);
    }
    else if (s == 1)
    {
        // Rhodes: a sine that barks when hit hard (phase modulation that fades), plus a short tine ping
        v.inc[0] = f0 / sr;
        v.amp[0] = 1.0f;
        v.env[0] = 1.0f;
        v.mul[0] = decayMul (0.8 + get (decay) * 5.0);

        if (f0 * 7.0 < nyquist)
        {
            v.inc[1] = f0 * 7.0 / sr;
            v.amp[1] = 0.12f * chr * velocity;
            v.env[1] = 1.0f;
            v.mul[1] = decayMul (0.05);
        }

        v.numUsed = 2;
        v.bark = (0.2f + chr * 2.6f) * velocity;
        v.barkMul = decayMul (0.12);
    }
    else
    {
        // Organ: drawbars at 16', 8', 4', 2 2/3', 2', 1 3/5', 1'. Character pulls the upper ones in.
        const double ratios[] = { 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0 };
        const float levels[] = { 0.6f, 1.0f, chr, 0.8f * chr, 0.7f * chr, 0.4f * chr * chr, 0.45f * chr * chr };

        for (int k = 0; k < 7; ++k)
        {
            if (f0 * ratios[k] >= nyquist)
                continue;

            v.inc[(size_t) k] = f0 * ratios[k] / sr;
            v.amp[(size_t) k] = levels[k];
            v.env[(size_t) k] = 1.0f;
            v.mul[(size_t) k] = 1.0f;
        }

        v.numUsed = 7;
        v.attack = 0.0f;
        v.click = 0.12f * get (tone);
        v.clickMul = decayMul (0.002);
        v.gain = 0.6f + 0.4f * v.gain;   // organs barely respond to how hard you play
    }

    // Even loudness whatever the mix of partials
    float energy = 0.0f;

    for (int k = 0; k < v.numUsed; ++k)
        energy += v.amp[(size_t) k] * v.amp[(size_t) k];

    const float norm = 0.45f / std::sqrt (juce::jmax (0.0001f, energy));

    for (int k = 0; k < v.numUsed; ++k)
        v.amp[(size_t) k] *= norm;
}

void KngKeysPlugin::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && v.held && v.note == note)
            v.held = false;
}

void KngKeysPlugin::render (juce::AudioBuffer<float>& out, int start, int num)
{
    if (num <= 0 || sampleRate <= 0.0)
        return;

    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    const double sr = sampleRate;
    const float motionAmount = get (motion);
    const bool organ = juce::roundToInt (get (sound)) == 2;
    const double lfoStep = get (speed) / sr;
    const float cutoff = 800.0f * std::pow (20.0f, get (tone));
    const float lowCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / (float) sr);
    const float gain = juce::Decibels::decibelsToGain (get (level));

    // Release: how fast a let-go note fades (piano, Rhodes, organ)
    const float relMul[3] = { (float) std::exp (-1.0 / (0.25 * sr)), (float) std::exp (-1.0 / (0.4 * sr)), (float) std::exp (-1.0 / (0.06 * sr)) };
    const float attackStep = (float) (1.0 / (0.005 * sr));

    auto* left  = out.getWritePointer (0, start);
    auto* right = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

    for (int i = 0; i < num; ++i)
    {
        float sumL = 0.0f, sumR = 0.0f;

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            float x = 0.0f;

            if (v.sound == 1)
            {
                const double ph = v.phase[0] * twoPi;
                x = v.amp[0] * v.env[0] * (float) std::sin (ph + v.bark * std::sin (ph))
                  + v.amp[1] * v.env[1] * (float) std::sin (v.phase[1] * twoPi);
                v.bark *= v.barkMul;
            }
            else
            {
                for (int k = 0; k < v.numUsed; ++k)
                    x += v.amp[(size_t) k] * v.env[(size_t) k] * (float) std::sin (v.phase[(size_t) k] * twoPi);
            }

            for (int k = 0; k < v.numUsed; ++k)
            {
                v.env[(size_t) k] *= v.mul[(size_t) k];
                v.phase[(size_t) k] += v.inc[(size_t) k];

                if (v.phase[(size_t) k] >= 1.0)
                    v.phase[(size_t) k] -= 1.0;
            }

            // Hammer or key click
            if (v.click > 0.0001f)
            {
                x += v.click * (random.nextFloat() * 2.0f - 1.0f);
                v.click *= v.clickMul;
            }

            if (v.attack < 1.0f)
            {
                v.attack = juce::jmin (1.0f, v.attack + attackStep);
                x *= v.attack;
            }

            if (! v.held)
                v.rel *= relMul[v.sound];

            x *= v.rel * v.gain;

            const float angle = (v.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            sumL += x * std::cos (angle);
            sumR += x * std::sin (angle);

            if (v.rel < 0.0001f || (v.sound != 2 && v.env[0] < 0.0001f))
                v.active = false;
        }

        // Motion: Rhodes tremolo pans side to side, the organ's spinning speaker swirls
        if (motionAmount > 0.0f)
        {
            const float lfo = (float) std::sin (lfoPhase * twoPi);
            const float lfo90 = (float) std::cos (lfoPhase * twoPi);
            const float depth = motionAmount * 0.5f;
            sumL *= 1.0f - depth * (0.5f + 0.5f * lfo);
            sumR *= 1.0f - depth * (0.5f + 0.5f * (organ ? lfo90 : -lfo));
        }

        lfoPhase += lfoStep;

        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;

        // Tone: a gentle low pass
        lowState[0] += lowCoef * (sumL - lowState[0]);
        lowState[1] += lowCoef * (sumR - lowState[1]);

        left[i] += lowState[0] * gain;

        if (right != nullptr)
            right[i] += lowState[1] * gain;
        else
            left[i] += lowState[1] * gain;
    }
}

void KngKeysPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    juce::ScopedNoDenormals noDenormals;
    auto& out = *fc.destBuffer;

    for (int ch = 0; ch < out.getNumChannels(); ++ch)
        out.clear (ch, fc.bufferStartSample, fc.bufferNumSamples);

    // Stop pressed: let go of everything so it fades instead of ringing on
    if (wasPlaying && ! fc.isPlaying)
        for (auto& v : voices)
            v.held = false;

    wasPlaying = fc.isPlaying;

    int cursor = 0;

    auto renderUpTo = [&] (int end)
    {
        if (end > cursor)
            render (out, fc.bufferStartSample + cursor, end - cursor);

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
                noteOn (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff())
                noteOff (m.getNoteNumber());
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                for (auto& v : voices)
                    v.held = false;
        }
    }

    renderUpTo (fc.bufferNumSamples);
    publishLitNotes();

    float peak = 0.0f;

    for (int ch = 0; ch < out.getNumChannels(); ++ch)
        peak = juce::jmax (peak, out.getMagnitude (ch, fc.bufferStartSample, fc.bufferNumSamples));

    reportMeters (0.0f, peak, 0.0f);
}

std::unique_ptr<te::Plugin::EditorComponent> KngKeysPlugin::createEditor()
{
    return std::make_unique<KngKeysView> (*this);
}
