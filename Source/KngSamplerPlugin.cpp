#include "KngSamplerPlugin.h"

namespace te = tracktion;

const char* KngSamplerPlugin::xmlTypeName = "kngsampler";

KngSamplerPlugin::KngSamplerPlugin (te::PluginCreationInfo info)
    : te::Plugin (info)
{
    syncFromState();
}

KngSamplerPlugin::~KngSamplerPlugin()
{
    notifyListenersOfDeletion();
}

void KngSamplerPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    reset();
}

void KngSamplerPlugin::reset()
{
    for (auto& v : voices)
        v = {};
}

void KngSamplerPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    for (auto& id : { fileProp, rootProp, gainProp, attackProp, releaseProp, glideProp,
                      monoProp, oneShotProp, loopProp, loopStartProp, loopEndProp })
        if (v.hasProperty (id))
            state.setProperty (id, v.getProperty (id), nullptr);

    syncFromState();
}

//==============================================================================
// Message thread

void KngSamplerPlugin::readFile (const juce::File& file)
{
    loadedFile = file;
    std::shared_ptr<juce::AudioBuffer<float>> buffer;
    double rate = 44100.0;

    if (file.existsAsFile())
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        if (std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (file) })
        {
            // Cap at one minute so a long file can't eat all the memory
            const int length = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
            const int channels = juce::jmin (2, (int) reader->numChannels);

            buffer = std::make_shared<juce::AudioBuffer<float>> (channels, length);
            reader->read (buffer.get(), 0, length, 0, true, true);
            rate = reader->sampleRate;
        }
    }

    juce::SpinLock::ScopedLockType sl (lock);
    sample = buffer;
    sampleFileRate = rate;
}

void KngSamplerPlugin::syncFromState()
{
    Settings s;
    s.root      = juce::jlimit (0, 127, (int) state.getProperty (rootProp, 60));
    s.gainDb    = (float) state.getProperty (gainProp, 0.0f);
    s.attackMs  = (float) state.getProperty (attackProp, 2.0f);
    s.releaseMs = (float) state.getProperty (releaseProp, 30.0f);
    s.glideMs   = (float) state.getProperty (glideProp, 0.0f);
    s.mono      = (bool) state.getProperty (monoProp, false);
    s.oneShot   = (bool) state.getProperty (oneShotProp, false);
    s.loop      = (bool) state.getProperty (loopProp, false);
    s.loopStart = juce::jlimit (0.0f, 0.97f, (float) state.getProperty (loopStartProp, 0.4f));
    s.loopEnd   = juce::jlimit (s.loopStart + 0.02f, 1.0f, (float) state.getProperty (loopEndProp, 0.9f));

    // Reload when the file changes (a new sample, or an undo)
    const juce::File file (state.getProperty (fileProp).toString());

    if (file != loadedFile)
        readFile (file);

    juce::SpinLock::ScopedLockType sl (lock);
    settings = s;
}

bool KngSamplerPlugin::loadSample (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    state.setProperty (fileProp, file.getFullPathName(), nullptr);
    syncFromState();
    return getSampleForDisplay() != nullptr;
}

juce::File KngSamplerPlugin::getSampleFile() const
{
    return juce::File (state.getProperty (fileProp).toString());
}

std::shared_ptr<juce::AudioBuffer<float>> KngSamplerPlugin::getSampleForDisplay()
{
    juce::SpinLock::ScopedLockType sl (lock);
    return sample;
}

//==============================================================================
// Audio thread

void KngSamplerPlugin::noteOn (int note, float velocity, const Settings& s, double fileRate)
{
    const double inc = std::pow (2.0, (note - s.root) / 12.0) * fileRate / sampleRate;

    if (s.mono)
    {
        auto& v = voices[0];
        const bool legato = v.active && ! v.releasing && ! v.fast;

        // Overlapping notes with glide on: slide to the new pitch without retriggering
        if (legato && s.glideMs > 0.0f)
        {
            v.note = note;
            v.vel = velocity;
            v.targetInc = inc;
            return;
        }

        // Mono cut: fade the old note out quickly instead of clicking
        if (v.active)
        {
            voices[1] = v;
            voices[1].fast = true;
        }

        v = {};
        v.active = true;
        v.note = note;
        v.vel = velocity;
        v.inc = v.targetInc = inc;
        return;
    }

    // The same note again: fade the old one out
    for (auto& v : voices)
        if (v.active && v.note == note && ! v.fast)
            v.fast = true;

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
        target = &voices[0];

    *target = {};
    target->active = true;
    target->note = note;
    target->vel = velocity;
    target->inc = target->targetInc = inc;
}

void KngSamplerPlugin::noteOff (int note, const Settings& s)
{
    if (s.oneShot)
        return;

    for (auto& v : voices)
        if (v.active && v.note == note && ! v.releasing)
            v.releasing = true;
}

void KngSamplerPlugin::renderVoices (juce::AudioBuffer<float>& out, int start, int num, const Settings& s,
                                     const juce::AudioBuffer<float>& smp, double fileRate)
{
    const int len = smp.getNumSamples();
    const int chans = smp.getNumChannels();

    if (num <= 0 || len < 4)
        return;

    const float gain = juce::Decibels::decibelsToGain (s.gainDb);
    const float sr = (float) sampleRate;
    const float attackStep  = 1.0f / juce::jmax (1.0f, s.attackMs * 0.001f * sr);
    const float releaseStep = 1.0f / juce::jmax (1.0f, s.releaseMs * 0.001f * sr);
    const float fastStep    = 1.0f / juce::jmax (1.0f, 0.005f * sr);
    const double glideCoef  = s.glideMs > 0.0f ? 1.0 - std::exp (-1.0 / (s.glideMs * 0.001 * sampleRate)) : 1.0;

    // Loop region, in sample positions
    const double loopStart = juce::jmax (1.0, (double) s.loopStart * (len - 2));
    const double loopEnd   = juce::jlimit (loopStart + 64.0, (double) (len - 2), (double) s.loopEnd * (len - 2));
    const double loopLen   = loopEnd - loopStart;
    const double xfade     = juce::jmin (loopLen * 0.25, 0.02 * fileRate, loopStart);

    auto* left  = out.getWritePointer (0, start);
    auto* right = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

    auto read = [&] (int ch, double p) -> float
    {
        const int i = (int) p;

        if (i < 0 || i >= len - 1)
            return 0.0f;

        const float frac = (float) (p - i);
        const float* d = smp.getReadPointer (juce::jmin (ch, chans - 1));
        return d[i] + frac * (d[i + 1] - d[i]);
    };

    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        for (int i = 0; i < num; ++i)
        {
            // Envelope
            if (v.fast)
            {
                v.env -= fastStep;
            }
            else if (v.releasing)
            {
                v.env -= releaseStep;
            }
            else if (v.env < 1.0f)
            {
                v.env = juce::jmin (1.0f, v.env + attackStep);
            }

            if (v.env <= 0.0f && (v.fast || v.releasing))
            {
                v.active = false;
                break;
            }

            // Glide toward the target pitch
            v.inc += (v.targetInc - v.inc) * glideCoef;

            const bool looping = s.loop && ! v.releasing && ! v.fast;
            float l = read (0, v.pos);
            float r = read (1, v.pos);

            // Crossfade into the loop start so the loop never clicks
            if (looping && xfade > 1.0 && v.pos > loopEnd - xfade)
            {
                const float blend = (float) ((v.pos - (loopEnd - xfade)) / xfade);
                l = l * (1.0f - blend) + read (0, v.pos - loopLen) * blend;
                r = r * (1.0f - blend) + read (1, v.pos - loopLen) * blend;
            }

            const float amp = v.env * v.vel * gain;
            left[i] += l * amp;

            if (right != nullptr)
                right[i] += r * amp;

            v.pos += v.inc;

            if (looping && v.pos >= loopEnd)
                v.pos -= loopLen;

            if (v.pos >= len - 1)
            {
                v.active = false;
                break;
            }
        }
    }
}

void KngSamplerPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    auto& out = *fc.destBuffer;

    Settings s;
    std::shared_ptr<juce::AudioBuffer<float>> smp;
    double fileRate = 44100.0;

    {
        juce::SpinLock::ScopedLockType sl (lock);
        s = settings;
        smp = sample;
        fileRate = sampleFileRate;
    }

    for (int ch = 0; ch < out.getNumChannels(); ++ch)
        out.clear (ch, fc.bufferStartSample, fc.bufferNumSamples);

    int cursor = 0;

    auto renderUpTo = [&] (int end)
    {
        if (smp != nullptr && end > cursor)
            renderVoices (out, fc.bufferStartSample + cursor, end - cursor, s, *smp, fileRate);

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
                noteOn (m.getNoteNumber(), m.getFloatVelocity(), s, fileRate);
            else if (m.isNoteOff())
                noteOff (m.getNoteNumber(), s);
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                for (auto& v : voices)
                    if (v.active)
                        v.fast = true;
        }
    }

    renderUpTo (fc.bufferNumSamples);
}
