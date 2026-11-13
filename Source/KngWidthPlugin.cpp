#include "KngWidthPlugin.h"
#include "KngWidthView.h"

namespace te = tracktion;

const char* KngWidthPlugin::xmlTypeName = "kngwidth";

static std::vector<KngParamDef> widthParams()
{
    return {
        { "width",    "Width",      0.0f,   2.0f,   1.0f,   0.0f, 0.0f, "%",  nullptr },
        { "monoBass", "Mono Bass", 20.0f, 400.0f, 120.0f, 120.0f, 0.0f, "Hz", nullptr },
        { "haas",     "Haas",       0.0f,  30.0f,   0.0f,   0.0f, 0.0f, "ms", nullptr },
        { "output",   "Output",   -12.0f,   6.0f,   0.0f,   0.0f, 0.0f, "dB", nullptr }
    };
}

static std::vector<KngPresetDef> widthPresets()
{
    using P = KngWidthPlugin;

    return {
        { "Default", {} },
        { "Wide Hook",    { { P::width, 1.5f } } },
        { "Huge",         { { P::width, 1.8f }, { P::monoBass, 150.0f }, { P::output, -1.0f } } },
        { "Narrow Verse", { { P::width, 0.7f } } },
        { "Mono Check",   { { P::width, 0.0f } } },
        { "Haas Spread",  { { P::width, 1.2f }, { P::haas, 12.0f } } }
    };
}

KngWidthPlugin::KngWidthPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, widthParams(), widthPresets())
{
}

void KngWidthPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    haasBuf.assign ((size_t) (0.05 * sampleRate) + 2, 0.0f);
    lastRate = 0.0;
    reset();
}

void KngWidthPlugin::reset()
{
    sideLow.reset();
    std::fill (haasBuf.begin(), haasBuf.end(), 0.0f);
    haasPos = 0;
}

void KngWidthPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0 || haasBuf.empty())
        return;

    auto& buffer = *fc.destBuffer;

    // Width needs two sides
    if (buffer.getNumChannels() < 2)
        return;

    juce::ScopedNoDenormals noDenormals;

    if (get (monoBass) != lastMonoBass || sampleRate != lastRate)
    {
        sideLow.lowPass (sampleRate, get (monoBass));
        lastMonoBass = get (monoBass);
        lastRate = sampleRate;
    }

    const float w = get (width);
    const bool bassToMono = get (monoBass) > 21.0f;
    const int haasSamples = juce::jlimit (0, (int) haasBuf.size() - 1, (int) (get (haas) * 0.001 * sampleRate));
    const float outGain = juce::Decibels::decibelsToGain (get (output));
    const int n = (int) haasBuf.size();

    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = buffer.getWritePointer (1, fc.bufferStartSample);

    float inMax = 0.0f, outMax = 0.0f;
    double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float l = left[i];
        const float rIn = right[i];
        float r = rIn;

        // Haas: the right side a few milliseconds late, which spreads a mono sound
        haasBuf[(size_t) haasPos] = r;

        if (haasSamples > 0)
        {
            int p = haasPos - haasSamples;

            if (p < 0)
                p += n;

            r = haasBuf[(size_t) p];
        }

        if (++haasPos >= n)
            haasPos = 0;

        // Mid / side
        const float mid = 0.5f * (l + r);
        float side = 0.5f * (l - r) * w;

        // Keep the lows in the middle
        if (bassToMono)
            side -= sideLow.process (side);

        const float ol = (mid + side) * outGain;
        const float orr = (mid - side) * outGain;

        left[i] = ol;
        right[i] = orr;

        inMax = juce::jmax (inMax, std::abs (l), std::abs (rIn));
        outMax = juce::jmax (outMax, std::abs (ol), std::abs (orr));
        sumLR += ol * orr;
        sumLL += ol * ol;
        sumRR += orr * orr;

        // Every 4th sample goes to the scope
        if (++scopeSkip >= 4)
        {
            scopeSkip = 0;
            const int sp = scopePos.load();
            scopeL[(size_t) sp].store (ol, std::memory_order_relaxed);
            scopeR[(size_t) sp].store (orr, std::memory_order_relaxed);
            scopePos.store ((sp + 1) % scopeSize);
        }
    }

    // How alike the two sides are: +1 is mono, 0 is very wide, below 0 can vanish in mono
    const double denom = std::sqrt (sumLL * sumRR);

    if (denom > 1.0e-9)
    {
        const float now = (float) (sumLR / denom);
        correlation = correlation.load() + (now - correlation.load()) * 0.3f;
    }

    reportMeters (inMax, outMax, 0.0f);
}

std::unique_ptr<te::Plugin::EditorComponent> KngWidthPlugin::createEditor()
{
    return std::make_unique<KngWidthView> (*this);
}
