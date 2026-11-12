#include "Kng808Plugin.h"
#include "Kng808View.h"

namespace te = tracktion;

const char* Kng808Plugin::xmlTypeName = "kng808";

static std::vector<KngParamDef> params808()
{
    return {
        { "tune",     "Tune",        -12.0f,   12.0f,    0.0f,   0.0f, 1.0f, "st", nullptr },
        { "drop",     "Pitch Drop",    0.0f,   24.0f,    7.0f,   0.0f, 1.0f, "st", nullptr },
        { "dropTime", "Drop Time",     5.0f,  300.0f,   40.0f,  60.0f, 0.0f, "ms", nullptr },
        { "length",   "Length",       50.0f, 4000.0f, 1200.0f, 700.0f, 0.0f, "ms", nullptr },
        { "hold",     "Hold Notes",    0.0f,    1.0f,    1.0f,   0.0f, 1.0f, "",   "toggle" },
        { "punch",    "Punch",         0.0f,    1.0f,    0.4f,   0.0f, 0.0f, "%",  nullptr },
        { "drive",    "Drive",         0.0f,    1.0f,    0.2f,   0.0f, 0.0f, "%",  nullptr },
        { "glide",    "Glide",         0.0f,  500.0f,   60.0f, 100.0f, 0.0f, "ms", nullptr },
        { "level",    "Level",       -24.0f,    6.0f,   -3.0f,  -9.0f, 0.0f, "dB", nullptr }
    };
}

static std::vector<KngPresetDef> presets808()
{
    using P = Kng808Plugin;

    return {
        { "Default", {} },
        { "Classic 808", { { P::drop, 5.0f },  { P::dropTime, 30.0f }, { P::length, 1500.0f }, { P::punch, 0.3f }, { P::drive, 0.1f } } },
        { "Hard 808",    { { P::drop, 12.0f }, { P::dropTime, 25.0f }, { P::length, 1200.0f }, { P::punch, 0.7f }, { P::drive, 0.55f } } },
        { "Long Slide",  { { P::drop, 3.0f },  { P::length, 3000.0f }, { P::glide, 140.0f }, { P::drive, 0.25f } } },
        { "Short Knock", { { P::drop, 14.0f }, { P::dropTime, 20.0f }, { P::length, 350.0f }, { P::hold, 0.0f }, { P::punch, 0.8f }, { P::drive, 0.35f } } },
        { "Distorted",   { { P::length, 1800.0f }, { P::punch, 0.5f }, { P::drive, 0.9f }, { P::level, -6.0f } } },
        { "Deep Sub",    { { P::drop, 2.0f },  { P::length, 2500.0f }, { P::punch, 0.1f }, { P::drive, 0.0f } } }
    };
}

Kng808Plugin::Kng808Plugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, params808(), presets808())
{
}

void Kng808Plugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    reset();
}

void Kng808Plugin::reset()
{
    for (auto& v : voices)
        v = {};
}

Kng808Plugin::Block Kng808Plugin::readBlock()
{
    const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    Block b;

    b.tuneSt   = get (tune);
    b.dropSt   = get (drop);
    b.dropTauS = get (dropTime) * 0.001f / 3.0f;   // most of the drop is done by Drop Time

    // Length: the time it takes to fade to silence (-60 dB)
    b.decayMul = (float) std::exp (-6.9 / (std::max (0.01, (double) get (length) * 0.001) * sr));

    b.punchAmt = get (punch);
    b.driveAmt = get (drive);

    const double glideS = get (glide) * 0.001;
    b.glideCoef = glideS > 0.0005 ? (float) (1.0 - std::exp (-1.0 / (glideS / 3.0 * sr))) : 1.0f;

    b.gain = juce::Decibels::decibelsToGain (get (level));
    b.holdOn = get (hold) > 0.5f;
    return b;
}

void Kng808Plugin::noteOn (int note, float velocity, const Block& b)
{
    auto& v = voices[0];

    // A note still held when the next one starts: slide to the new pitch without restarting
    if (v.active && v.held && ! v.releasing && ! v.fast && b.glideCoef < 1.0f)
    {
        v.note = note;
        v.target = note;
        return;
    }

    // The old note fades out quickly so the new one never clicks
    if (v.active)
    {
        voices[1] = v;
        voices[1].fast = true;
    }

    v = {};
    v.active = true;
    v.held = true;
    v.note = note;
    v.vel = 0.35f + 0.65f * velocity;
    v.pitch = v.target = note;
}

void Kng808Plugin::noteOff (int note, const Block& b)
{
    auto& v = voices[0];

    if (! v.active || v.note != note)
        return;

    v.held = false;

    // Hold Notes on: the 808 stops when the note ends. Off: it rings out its full length.
    if (b.holdOn)
        v.releasing = true;
}

void Kng808Plugin::render (juce::AudioBuffer<float>& out, int start, int num, const Block& b)
{
    if (num <= 0 || sampleRate <= 0.0)
        return;

    const double dt = 1.0 / sampleRate;
    const float relStep  = 1.0f / juce::jmax (1.0f, (float) (0.04 * sampleRate));
    const float fastStep = 1.0f / juce::jmax (1.0f, (float) (0.005 * sampleRate));
    const float driveGain = 1.0f + b.driveAmt * 8.0f;
    const float driveNorm = 1.0f / std::tanh (driveGain);
    const double dropTau = juce::jmax (0.0005, (double) b.dropTauS);
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    auto* left  = out.getWritePointer (0, start);
    auto* right = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

    for (int i = 0; i < num; ++i)
    {
        float sum = 0.0f;

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            v.pitch += (v.target - v.pitch) * b.glideCoef;
            const double dropNow = b.dropSt * std::exp (-v.t / dropTau);
            const double freq = 440.0 * std::exp2 ((v.pitch + b.tuneSt + dropNow - 69.0) / 12.0);

            v.phase += freq * dt;

            if (v.phase >= 1.0)
                v.phase -= std::floor (v.phase);

            // The body, a punchy start, and a tiny click on top
            const float body = (float) std::sin (v.phase * twoPi);
            const float knock = 1.0f + b.punchAmt * 0.8f * (float) std::exp (-v.t / 0.012);
            const float click = b.punchAmt * 0.3f * (float) (std::exp (-v.t / 0.003) * std::sin (v.t * twoPi * 1800.0));

            sum += (body * knock + click) * 0.6f * v.env * v.rel * v.vel;

            v.env *= b.decayMul;
            v.t += dt;

            if (v.fast)
                v.rel -= fastStep;
            else if (v.releasing)
                v.rel -= relStep;

            if (v.rel <= 0.0f || v.env < 0.0001f)
                v.active = false;
        }

        // Drive: warm at low settings, dirty at the top
        const float y = (b.driveAmt > 0.001f ? std::tanh (sum * driveGain) * driveNorm : sum) * b.gain;
        left[i] += y;

        if (right != nullptr)
            right[i] += y;
    }
}

void Kng808Plugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    juce::ScopedNoDenormals noDenormals;
    auto& out = *fc.destBuffer;
    const auto b = readBlock();

    for (int ch = 0; ch < out.getNumChannels(); ++ch)
        out.clear (ch, fc.bufferStartSample, fc.bufferNumSamples);

    // Stop pressed: fade out in about 5 ms instead of ringing on
    if (wasPlaying && ! fc.isPlaying)
        for (auto& v : voices)
            if (v.active)
                v.fast = true;

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
                noteOn (m.getNoteNumber(), m.getFloatVelocity(), b);
            else if (m.isNoteOff())
                noteOff (m.getNoteNumber(), b);
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                for (auto& v : voices)
                    if (v.active)
                        v.fast = true;
        }
    }

    renderUpTo (fc.bufferNumSamples);

    displayNote = voices[0].active ? voices[0].note : -1;

    // Level for the panel's meter
    float peak = 0.0f;

    for (int ch = 0; ch < out.getNumChannels(); ++ch)
        peak = juce::jmax (peak, out.getMagnitude (ch, fc.bufferStartSample, fc.bufferNumSamples));

    reportMeters (0.0f, peak, 0.0f);
}

std::unique_ptr<te::Plugin::EditorComponent> Kng808Plugin::createEditor()
{
    return std::make_unique<Kng808View> (*this);
}

void Kng808Plugin::drawDisplay (juce::Graphics& g, juce::Rectangle<float> area, const KngHistory&)
{
    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (area, 6.0f);

    auto plot = area.reduced (16.0f).withTrimmedTop (20.0f);
    const float lengthS = get (length) * 0.001f;
    const float span = juce::jmax (0.3f, lengthS * 1.1f);
    const float dropSt = get (drop);
    const float tau = juce::jmax (0.0005f, get (dropTime) * 0.001f / 3.0f);

    juce::Path volume, pitch;

    for (int k = 0; k <= 200; ++k)
    {
        const float t = span * (float) k / 200.0f;
        const float px = plot.getX() + plot.getWidth() * (float) k / 200.0f;
        const float amp = std::exp (-6.9f * t / juce::jmax (0.01f, lengthS));
        const float drop01 = dropSt * std::exp (-t / tau) / 24.0f;

        const float vy = plot.getBottom() - amp * plot.getHeight();
        const float py = plot.getBottom() - drop01 * plot.getHeight() * 0.9f;

        if (k == 0)
        {
            volume.startNewSubPath (px, vy);
            pitch.startNewSubPath (px, py);
        }
        else
        {
            volume.lineTo (px, vy);
            pitch.lineTo (px, py);
        }
    }

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.strokePath (volume, juce::PathStrokeType (2.0f));
    g.setColour (juce::Colour (0xffe6823a));
    g.strokePath (pitch, juce::PathStrokeType (2.0f));

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("Length " + juce::String (lengthS, 2) + " s    Drop " + juce::String (juce::roundToInt (dropSt))
                    + " st    Glide " + juce::String (juce::roundToInt (get (glide))) + " ms",
                area.reduced (12.0f).toNearestInt(), juce::Justification::topLeft);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("white: volume    orange: pitch", area.reduced (12.0f).toNearestInt(), juce::Justification::topRight);
}
