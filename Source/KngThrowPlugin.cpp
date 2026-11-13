#include "KngThrowPlugin.h"
#include "KngThrowView.h"

namespace te = tracktion;

const char* KngThrowPlugin::xmlTypeName = "kngthrow";

static std::vector<KngParamDef> throwParams()
{
    return {
        { "time",     "Time",        0.0f,     5.0f,    3.0f,    0.0f, 1.0f, "",   "1/16|1/8|1/8 Dot|1/4|1/4 Dot|1/2" },
        { "feedback", "Feedback",    0.0f,     0.9f,   0.45f,    0.0f, 0.0f, "%",  nullptr },
        { "duck",     "Duck",        0.0f,     1.0f,    0.8f,    0.0f, 0.0f, "%",  nullptr },
        { "tone",     "Tone",     1000.0f, 12000.0f, 4500.0f, 4000.0f, 0.0f, "Hz", nullptr },
        { "wash",     "Wash",        0.0f,     1.0f,    0.3f,    0.0f, 0.0f, "%",  nullptr },
        { "pingPong", "Ping-Pong",   0.0f,     1.0f,    0.0f,    0.0f, 1.0f, "",   "toggle" },
        { "mix",      "Mix",         0.0f,     1.0f,   0.35f,    0.0f, 0.0f, "%",  nullptr }
    };
}

static std::vector<KngPresetDef> throwPresets()
{
    using P = KngThrowPlugin;

    return {
        { "Default", {} },
        { "End of Line",   { { P::time, 3.0f }, { P::feedback, 0.4f }, { P::duck, 0.9f } } },
        { "Slap Throw",    { { P::time, 0.0f }, { P::feedback, 0.2f }, { P::duck, 0.6f }, { P::wash, 0.0f } } },
        { "Dotted Bounce", { { P::time, 2.0f }, { P::feedback, 0.5f }, { P::pingPong, 1.0f } } },
        { "Big Wash",      { { P::time, 5.0f }, { P::feedback, 0.6f }, { P::wash, 0.8f }, { P::duck, 0.9f } } },
        { "Ad-lib Echo",   { { P::time, 1.0f }, { P::feedback, 0.35f }, { P::pingPong, 1.0f }, { P::mix, 0.45f } } }
    };
}

float KngThrowPlugin::beatsFor (int t)
{
    static const float beats[] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
    return beats[juce::jlimit (0, 5, t)];
}

KngThrowPlugin::KngThrowPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, throwParams(), throwPresets())
{
    bpmTimer.onTick = [this]
    {
        bpm = (float) edit.tempoSequence.getBpmAt (edit.getTransport().getPosition());
    };

    bpmTimer.onTick();
    bpmTimer.startTimerHz (4);
}

KngThrowPlugin::~KngThrowPlugin()
{
    bpmTimer.stopTimer();
    notifyListenersOfDeletion();
}

void KngThrowPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;

    // Room for a half note at 40 BPM
    for (auto& line : lines)
        line.assign ((size_t) (3.2 * sampleRate) + 4, 0.0f);

    const double scale = sampleRate / 44100.0;
    diffusers[0][0].setSize ((int) (347 * scale));
    diffusers[0][1].setSize ((int) (113 * scale));
    diffusers[1][0].setSize ((int) (379 * scale));
    diffusers[1][1].setSize ((int) (127 * scale));

    lastRate = 0.0;
    reset();
}

void KngThrowPlugin::reset()
{
    for (auto& line : lines)
        std::fill (line.begin(), line.end(), 0.0f);

    for (auto& pair : diffusers)
        for (auto& ap : pair)
            ap.clear();

    for (int c = 0; c < 2; ++c)
    {
        toneLp[(size_t) c].reset();
        toneHp[(size_t) c].reset();
    }

    writePos = 0;
    currentDelay = 0.0;
    duckEnv = 0.0f;
}

float KngThrowPlugin::readDelay (const std::vector<float>& buf, double delaySamples) const
{
    const int n = (int) buf.size();
    double p = writePos - delaySamples;

    while (p < 0.0)
        p += n;

    const int i0 = (int) p % n;
    const int i1 = (i0 + 1) % n;
    const float frac = (float) (p - std::floor (p));
    return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
}

void KngThrowPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0 || lines[0].empty())
        return;

    juce::ScopedNoDenormals noDenormals;

    if (get (tone) != lastTone || sampleRate != lastRate)
    {
        for (int c = 0; c < 2; ++c)
        {
            toneLp[(size_t) c].lowPass (sampleRate, get (tone));
            toneHp[(size_t) c].highPass (sampleRate, 150.0);
        }

        lastTone = get (tone);
        lastRate = sampleRate;
    }

    const int n = (int) lines[0].size();
    const double beatSeconds = 60.0 / juce::jlimit (40.0, 300.0, (double) bpm.load());
    const double target = juce::jlimit (1.0, (double) n - 2.0, beatsFor (juce::roundToInt (get (time))) * beatSeconds * sampleRate);

    if (currentDelay <= 0.0)
        currentDelay = target;

    const float fb = get (feedback), duckAmount = get (duck), washAmount = get (wash), mixAmount = get (mix);
    const bool ping = get (pingPong) > 0.5f;
    const float duckAttack = KngDsp::Follower::coef (5.0, sampleRate);
    const float duckRelease = KngDsp::Follower::coef (250.0, sampleRate);

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = chans > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;
    auto& lineL = lines[0];
    auto& lineR = lines[1];

    float inMax = 0.0f, outMax = 0.0f, duckMax = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float l = left[i];
        const float r = right != nullptr ? right[i] : l;

        // While you're singing, the echoes back off
        const float level = juce::jmax (std::abs (l), std::abs (r));
        duckEnv += (level - duckEnv) * (level > duckEnv ? duckAttack : duckRelease);
        const float singing = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (duckEnv, -100.0f) + 50.0f) / 30.0f);
        const float duckGain = 1.0f - duckAmount * singing;

        // A tempo change glides the time instead of clicking
        currentDelay += (target - currentDelay) * 0.0005;

        const float el = toneHp[0].process (toneLp[0].process (readDelay (lineL, currentDelay)));
        const float er = toneHp[1].process (toneLp[1].process (readDelay (lineR, currentDelay)));

        if (ping)
        {
            lineL[(size_t) writePos] = 0.5f * (l + r) + er * fb;
            lineR[(size_t) writePos] = el * fb;
        }
        else
        {
            lineL[(size_t) writePos] = l + el * fb;
            lineR[(size_t) writePos] = r + er * fb;
        }

        if (++writePos >= n)
            writePos = 0;

        // Wash smears the repeats into a short tail
        float wl = el, wr = er;

        if (washAmount > 0.0f)
        {
            wl = el * (1.0f - washAmount) + diffusers[0][1].process (diffusers[0][0].process (el, 0.6f), 0.6f) * washAmount;
            wr = er * (1.0f - washAmount) + diffusers[1][1].process (diffusers[1][0].process (er, 0.6f), 0.6f) * washAmount;
        }

        const float wetGain = mixAmount * duckGain;

        if (right != nullptr)
        {
            left[i] = l + wl * wetGain;
            right[i] = r + wr * wetGain;
            outMax = juce::jmax (outMax, std::abs (left[i]), std::abs (right[i]));
        }
        else
        {
            left[i] = l + 0.5f * (wl + wr) * wetGain;
            outMax = juce::jmax (outMax, std::abs (left[i]));
        }

        inMax = juce::jmax (inMax, std::abs (l), std::abs (r));
        duckMax = juce::jmax (duckMax, -juce::Decibels::gainToDecibels (juce::jmax (0.001f, duckGain)));
    }

    reportMeters (inMax, outMax, duckMax);
}

std::unique_ptr<te::Plugin::EditorComponent> KngThrowPlugin::createEditor()
{
    return std::make_unique<KngThrowView> (*this);
}
