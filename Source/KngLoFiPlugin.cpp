#include "KngLoFiPlugin.h"
#include "KngLoFiView.h"

namespace te = tracktion;

const char* KngLoFiPlugin::xmlTypeName = "knglofi";

static std::vector<KngParamDef> loFiParams()
{
    return {
        { "bits",   "Bits",      4.0f,    16.0f,    12.0f,    0.0f, 0.0f, "",   nullptr },
        { "rate",   "Rate",   2000.0f, 44100.0f, 22000.0f, 11000.0f, 0.0f, "Hz", nullptr },
        { "wobble", "Wobble",    0.0f,     1.0f,     0.3f,    0.0f, 0.0f, "%",  nullptr },
        { "dust",   "Dust",      0.0f,     1.0f,     0.2f,    0.0f, 0.0f, "%",  nullptr },
        { "tone",   "Tone",   1000.0f, 20000.0f,  8000.0f, 5000.0f, 0.0f, "Hz", nullptr },
        { "mix",    "Mix",       0.0f,     1.0f,     1.0f,    0.0f, 0.0f, "%",  nullptr }
    };
}

static std::vector<KngPresetDef> loFiPresets()
{
    using P = KngLoFiPlugin;

    return {
        { "Default", {} },
        { "Dusty Keys",     { { P::bits, 12.0f }, { P::wobble, 0.4f }, { P::dust, 0.4f }, { P::tone, 6000.0f } } },
        { "Old Sampler",    { { P::bits, 8.0f }, { P::rate, 12000.0f }, { P::wobble, 0.0f }, { P::dust, 0.0f } } },
        { "Warped Tape",    { { P::wobble, 0.8f }, { P::dust, 0.15f }, { P::tone, 7000.0f } } },
        { "Vinyl Bedroom",  { { P::bits, 14.0f }, { P::wobble, 0.2f }, { P::dust, 0.6f }, { P::tone, 5000.0f } } },
        { "Crushed",        { { P::bits, 5.0f }, { P::rate, 6000.0f }, { P::mix, 0.7f } } }
    };
}

KngLoFiPlugin::KngLoFiPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, loFiParams(), loFiPresets())
{
}

void KngLoFiPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;

    for (auto& line : lines)
        line.assign ((size_t) (0.03 * sampleRate) + 4, 0.0f);

    lastRate = 0.0;
    reset();
}

void KngLoFiPlugin::reset()
{
    for (auto& line : lines)
        std::fill (line.begin(), line.end(), 0.0f);

    writePos = 0;
    held = {};
    holdCount = 0.0;

    for (auto& f : toneLp)
        f.reset();
}

float KngLoFiPlugin::readDelay (int channel, double delaySamples) const
{
    const auto& buf = lines[(size_t) channel];
    const int n = (int) buf.size();
    double p = writePos - delaySamples;

    while (p < 0.0)
        p += n;

    const int i0 = (int) p % n;
    const int i1 = (i0 + 1) % n;
    const float frac = (float) (p - std::floor (p));
    return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
}

void KngLoFiPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0 || lines[0].empty())
        return;

    juce::ScopedNoDenormals noDenormals;

    if (get (tone) != lastTone || sampleRate != lastRate)
    {
        for (auto& f : toneLp)
            f.lowPass (sampleRate, get (tone), 0.6);

        lastTone = get (tone);
        lastRate = sampleRate;
    }

    const float steps = std::pow (2.0f, get (bits) - 1.0f);
    const double holdLength = juce::jmax (1.0, sampleRate / juce::jmax (1000.0, (double) get (rate)));
    const float wobbleAmount = get (wobble), dustAmount = get (dust), mixAmount = get (mix);
    const int n = (int) lines[0].size();
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    // Wow is slow and deep (0.6 Hz), flutter is quick and small (7 Hz)
    const double wowStep = 0.6 / sampleRate, flutterStep = 7.0 / sampleRate;
    const double centre = 0.012 * sampleRate;
    const double wowDepth = wobbleAmount * 0.006 * sampleRate;
    const double flutterDepth = wobbleAmount * 0.0004 * sampleRate;

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = chans > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;

    float inMax = 0.0f, outMax = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float in[2] = { left[i], right != nullptr ? right[i] : left[i] };

        lines[0][(size_t) writePos] = in[0];
        lines[1][(size_t) writePos] = in[1];

        const double delay = centre + wowDepth * std::sin (wowPhase * twoPi) + flutterDepth * std::sin (flutterPhase * twoPi);

        // Lower sample rate: hold each value for a while
        holdCount += 1.0;
        const bool take = holdCount >= holdLength;

        if (take)
            holdCount -= holdLength;

        // Dust: rare clicks plus a little hiss
        float dustNow = 0.0f;

        if (dustAmount > 0.0f)
        {
            dustNow = (random.nextFloat() * 2.0f - 1.0f) * dustAmount * 0.006f;

            if (random.nextFloat() < dustAmount * 0.0004f)
                dustNow += (random.nextFloat() * 2.0f - 1.0f) * dustAmount * 0.35f;
        }

        float out[2];

        for (int c = 0; c < 2; ++c)
        {
            float x = wobbleAmount > 0.0f ? readDelay (c, juce::jlimit (1.0, (double) n - 2.0, delay)) : in[c];

            if (take)
                held[(size_t) c] = std::round (x * steps) / steps;

            x = toneLp[(size_t) c].process (held[(size_t) c] + dustNow);
            out[c] = in[c] * (1.0f - mixAmount) + x * mixAmount;
        }

        if (++writePos >= n)
            writePos = 0;

        wowPhase += wowStep;
        flutterPhase += flutterStep;

        if (wowPhase >= 1.0)
            wowPhase -= 1.0;

        if (flutterPhase >= 1.0)
            flutterPhase -= 1.0;

        left[i] = out[0];

        if (right != nullptr)
            right[i] = out[1];

        inMax = juce::jmax (inMax, std::abs (in[0]), std::abs (in[1]));
        outMax = juce::jmax (outMax, std::abs (out[0]), std::abs (out[1]));
    }

    reportMeters (inMax, outMax, 0.0f);
}

std::unique_ptr<te::Plugin::EditorComponent> KngLoFiPlugin::createEditor()
{
    return std::make_unique<KngLoFiView> (*this);
}
