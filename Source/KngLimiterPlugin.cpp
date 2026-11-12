#include "KngLimiterPlugin.h"
#include "KngLimiterView.h"

namespace te = tracktion;

const char* KngLimiterPlugin::xmlTypeName = "knglimiter";

static std::vector<KngParamDef> limiterParams()
{
    return {
        { "gain",    "Gain",      0.0f,   24.0f,   0.0f,   0.0f, 0.0f, "dB", nullptr },
        { "ceiling", "Ceiling", -12.0f,    0.0f,  -0.3f,   0.0f, 0.0f, "dB", nullptr },
        { "release", "Release",   1.0f, 1000.0f,  80.0f, 100.0f, 0.0f, "ms", nullptr },
        { "style",   "Style",     0.0f,    2.0f,   0.0f,   0.0f, 1.0f, "",   "Transparent|Punchy|Loud" }
    };
}

static std::vector<KngPresetDef> limiterPresets()
{
    using P = KngLimiterPlugin;

    return {
        { "Default", {} },
        { "Master Clean",    { { P::gain, 3.0f }, { P::ceiling, -0.3f }, { P::release, 120.0f }, { P::style, 0.0f } } },
        { "Master Loud",     { { P::gain, 8.0f }, { P::release, 60.0f }, { P::style, 2.0f } } },
        { "Beat Punch",      { { P::gain, 5.0f }, { P::release, 40.0f }, { P::style, 1.0f } } },
        { "Vocal Catch",     { { P::gain, 2.0f }, { P::ceiling, -1.0f }, { P::release, 50.0f } } },
        { "Streaming -1 dB", { { P::gain, 4.0f }, { P::ceiling, -1.0f }, { P::release, 100.0f } } }
    };
}

KngLimiterPlugin::KngLimiterPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, limiterParams(), limiterPresets())
{
}

void KngLimiterPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;

    // 1.5 ms look-ahead
    lookahead = juce::jmax (1, juce::roundToInt (0.0015 * sampleRate));

    for (auto& d : delayBuf)
        d.assign ((size_t) lookahead, 0.0f);

    holdBuf.assign ((size_t) lookahead, 1.0f);
    boxBuf.assign ((size_t) lookahead, 1.0f);
    reset();
}

void KngLimiterPlugin::reset()
{
    for (auto& d : delayBuf)
        std::fill (d.begin(), d.end(), 0.0f);

    std::fill (holdBuf.begin(), holdBuf.end(), 1.0f);
    std::fill (boxBuf.begin(), boxBuf.end(), 1.0f);
    boxSum = (double) boxBuf.size();
    releaseEnv = 1.0f;
    pos = 0;
}

void KngLimiterPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0 || holdBuf.empty())
        return;

    juce::ScopedNoDenormals noDenormals;

    const float inGain = juce::Decibels::decibelsToGain (get (gain));
    const float ceil = juce::Decibels::decibelsToGain (get (ceiling));
    const int s = juce::roundToInt (get (style));
    const float releaseSec = get (release) * 0.001f * (s == punchy ? 0.5f : 1.0f);
    const float sr = (float) sampleRate;
    const float baseRelease = 1.0f - std::exp (-1.0f / (releaseSec * sr));
    const int L = (int) holdBuf.size();

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = chans > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;

    float inMax = 0.0f, outMax = 0.0f, grMax = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        float xl = left[i] * inGain;
        float xr = (right != nullptr ? right[i] : left[i]) * inGain;

        // Loud: round the very tops off first, so the limiter has less to do
        if (s == loud)
        {
            const float c = ceil * 1.4f;
            xl = c * std::tanh (xl / c);
            xr = c * std::tanh (xr / c);
        }

        const float peak = juce::jmax (std::abs (xl), std::abs (xr));
        const float need = peak > ceil ? ceil / peak : 1.0f;

        // Transparent: lets go more slowly the harder it's working
        const float rel = s == transparent ? 1.0f - std::exp (-1.0f / (releaseSec * (1.0f + (1.0f - releaseEnv) * 4.0f) * sr))
                                           : baseRelease;

        releaseEnv = juce::jmin (need, releaseEnv + (1.0f - releaseEnv) * rel);

        // Hold the lowest gain across the look-ahead, then smooth over the same window,
        // so the gain is fully down exactly when the peak arrives
        holdBuf[(size_t) pos] = releaseEnv;
        float held = 1.0f;

        for (float hv : holdBuf)
            held = juce::jmin (held, hv);

        boxSum += held - boxBuf[(size_t) pos];
        boxBuf[(size_t) pos] = held;
        const float g = (float) (boxSum / L);

        const float dl = delayBuf[0][(size_t) pos];
        const float dr = delayBuf[1][(size_t) pos];
        delayBuf[0][(size_t) pos] = xl;
        delayBuf[1][(size_t) pos] = xr;

        if (++pos >= L)
            pos = 0;

        // The ceiling is never crossed
        const float ol = juce::jlimit (-ceil, ceil, dl * g);
        const float orr = juce::jlimit (-ceil, ceil, dr * g);

        left[i] = ol;

        if (right != nullptr)
            right[i] = orr;

        inMax = juce::jmax (inMax, peak);
        outMax = juce::jmax (outMax, std::abs (ol), std::abs (orr));
        grMax = juce::jmax (grMax, -juce::Decibels::gainToDecibels (g, -60.0f));
    }

    reportMeters (inMax, outMax, grMax);
}

std::unique_ptr<te::Plugin::EditorComponent> KngLimiterPlugin::createEditor()
{
    return std::make_unique<KngLimiterView> (*this);
}

void KngLimiterPlugin::drawDisplay (juce::Graphics& g, juce::Rectangle<float> area, const KngHistory& h)
{
    kngDrawHistory (g, area, h, 12.0f);

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("Ceiling " + juce::String (get (ceiling), 1) + " dB",
                area.reduced (12.0f).toNearestInt(), juce::Justification::topLeft);
}
