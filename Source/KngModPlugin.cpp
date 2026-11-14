#include "KngModPlugin.h"
#include "KngModView.h"

namespace te = tracktion;

const char* KngModPlugin::xmlTypeName = "kngmod";

static std::vector<KngParamDef> modParams()
{
    return {
        { "mode",     "Type",     0.0f, 2.0f,  0.0f, 0.0f, 1.0f, "",   "Chorus|Flanger|Phaser" },
        { "rate",     "Rate",    0.05f, 5.0f,  0.6f, 0.8f, 0.0f, "Hz", nullptr },
        { "depth",    "Depth",    0.0f, 1.0f,  0.5f, 0.0f, 0.0f, "%",  nullptr },
        { "feedback", "Feedback", 0.0f, 0.9f,  0.3f, 0.0f, 0.0f, "%",  nullptr },
        { "width",    "Width",    0.0f, 1.0f,  0.7f, 0.0f, 0.0f, "%",  nullptr },
        { "mix",      "Mix",      0.0f, 1.0f,  0.5f, 0.0f, 0.0f, "%",  nullptr }
    };
}

static std::vector<KngPresetDef> modPresets()
{
    using P = KngModPlugin;

    return {
        { "Default", {} },
        { "Lush Keys",      { { P::mode, 0.0f }, { P::rate, 0.4f }, { P::depth, 0.6f }, { P::width, 1.0f }, { P::mix, 0.45f } } },
        { "Jet Flanger",    { { P::mode, 1.0f }, { P::rate, 0.15f }, { P::depth, 0.8f }, { P::feedback, 0.7f }, { P::mix, 0.5f } } },
        { "Slow Phase",     { { P::mode, 2.0f }, { P::rate, 0.2f }, { P::depth, 0.7f }, { P::feedback, 0.4f } } },
        { "Fast Shimmer",   { { P::mode, 0.0f }, { P::rate, 3.5f }, { P::depth, 0.25f }, { P::mix, 0.35f } } },
        { "Guitar Swirl",   { { P::mode, 2.0f }, { P::rate, 1.2f }, { P::depth, 0.5f }, { P::feedback, 0.6f }, { P::mix, 0.6f } } }
    };
}

KngModPlugin::KngModPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, modParams(), modPresets())
{
}

void KngModPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;

    for (auto& line : lines)
        line.assign ((size_t) (0.05 * sampleRate) + 4, 0.0f);

    reset();
}

void KngModPlugin::reset()
{
    for (auto& line : lines)
        std::fill (line.begin(), line.end(), 0.0f);

    writePos = 0;
    fbState = {};

    for (auto& stages : apState)
        stages = {};
}

float KngModPlugin::readDelay (int channel, double delaySamples) const
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

void KngModPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0 || lines[0].empty())
        return;

    juce::ScopedNoDenormals noDenormals;

    const int m = juce::roundToInt (get (mode));
    const double step = get (rate) / sampleRate;
    const float depthAmount = get (depth), fb = get (feedback), mixAmount = get (mix);
    const double offset = get (width) * 0.25;   // the right side runs up to a quarter turn behind
    const int n = (int) lines[0].size();
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = chans > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;

    float inMax = 0.0f, outMax = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float in[2] = { left[i], right != nullptr ? right[i] : left[i] };
        float out[2] = { 0.0f, 0.0f };

        for (int c = 0; c < 2; ++c)
        {
            const double lfo = 0.5 + 0.5 * std::sin ((phase + (c == 1 ? offset : 0.0)) * twoPi);   // 0 to 1
            float wet = 0.0f;

            if (m == 2)
            {
                // Phaser: six all-pass stages swept across the mids, with feedback
                const double hz = 200.0 * std::pow (20.0, lfo * (0.3 + 0.7 * depthAmount));
                const double t = std::tan (juce::MathConstants<double>::pi * juce::jmin (hz, sampleRate * 0.45) / sampleRate);
                const float a = (float) ((t - 1.0) / (t + 1.0));
                float x = in[c] + fbState[(size_t) c] * fb;

                for (auto& s : apState[(size_t) c])
                {
                    const float y = a * x + s;
                    s = x - a * y;
                    x = y;
                }

                fbState[(size_t) c] = x;
                wet = x;
                lines[(size_t) c][(size_t) writePos] = in[c];
            }
            else
            {
                // Chorus: 7 to 25 ms. Flanger: 0.5 to 6 ms with feedback.
                const double ms = m == 0 ? 7.0 + 18.0 * depthAmount * lfo : 0.5 + 5.5 * depthAmount * lfo;
                const float d = readDelay (c, juce::jlimit (1.0, (double) n - 2.0, ms * 0.001 * sampleRate));
                const float feed = m == 1 ? fb : fb * 0.3f;
                lines[(size_t) c][(size_t) writePos] = in[c] + d * feed;
                wet = d;
            }

            out[c] = in[c] * (1.0f - mixAmount * 0.5f) + wet * mixAmount;
        }

        if (++writePos >= n)
            writePos = 0;

        phase += step;

        if (phase >= 1.0)
            phase -= 1.0;

        left[i] = out[0];

        if (right != nullptr)
            right[i] = out[1];

        inMax = juce::jmax (inMax, std::abs (in[0]), std::abs (in[1]));
        outMax = juce::jmax (outMax, std::abs (out[0]), std::abs (out[1]));
    }

    phaseNow = phase;
    reportMeters (inMax, outMax, 0.0f);
}

std::unique_ptr<te::Plugin::EditorComponent> KngModPlugin::createEditor()
{
    return std::make_unique<KngModView> (*this);
}
