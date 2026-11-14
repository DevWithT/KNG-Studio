#include "KngFilterPlugin.h"
#include "KngFilterView.h"

namespace te = tracktion;

const char* KngFilterPlugin::xmlTypeName = "kngfilter";

static std::vector<KngParamDef> filterParams()
{
    return {
        { "mode",     "Type",      0.0f,     2.0f,    0.0f,    0.0f, 1.0f, "",   "Low Pass|High Pass|Band Pass" },
        { "cutoff",   "Cutoff",   20.0f, 20000.0f, 2000.0f, 1000.0f, 0.0f, "Hz", nullptr },
        { "reso",     "Reso",      0.0f,     1.0f,   0.25f,    0.0f, 0.0f, "%",  nullptr },
        { "drive",    "Drive",     0.0f,     1.0f,    0.0f,    0.0f, 0.0f, "%",  nullptr },
        { "lfoRate",  "Wobble",    0.0f,     5.0f,    0.0f,    0.0f, 1.0f, "",   "Off|2 Bars|1 Bar|1/2|1/4|1/8" },
        { "lfoDepth", "Depth",     0.0f,     1.0f,    0.5f,    0.0f, 0.0f, "%",  nullptr },
        { "mix",      "Mix",       0.0f,     1.0f,    1.0f,    0.0f, 0.0f, "%",  nullptr }
    };
}

static std::vector<KngPresetDef> filterPresets()
{
    using P = KngFilterPlugin;

    return {
        { "Default", {} },
        { "Underwater Intro", { { P::cutoff, 400.0f }, { P::reso, 0.3f } } },
        { "Build-Up Sweep",   { { P::mode, 1.0f }, { P::cutoff, 200.0f }, { P::reso, 0.45f } } },
        { "Telephone Beat",   { { P::mode, 2.0f }, { P::cutoff, 1500.0f }, { P::reso, 0.5f } } },
        { "Wobble Bass",      { { P::cutoff, 600.0f }, { P::reso, 0.6f }, { P::drive, 0.4f }, { P::lfoRate, 4.0f }, { P::lfoDepth, 0.7f } } },
        { "Slow Breather",    { { P::cutoff, 1200.0f }, { P::reso, 0.35f }, { P::lfoRate, 1.0f }, { P::lfoDepth, 0.5f } } }
    };
}

static float wobbleBeats (int rate)
{
    static const float beats[] = { 0.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.5f };
    return beats[juce::jlimit (0, 5, rate)];
}

KngFilterPlugin::KngFilterPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, filterParams(), filterPresets())
{
    bpmTimer.onTick = [this]
    {
        bpm = (float) edit.tempoSequence.getBpmAt (edit.getTransport().getPosition());
    };

    bpmTimer.onTick();
    bpmTimer.startTimerHz (4);
}

KngFilterPlugin::~KngFilterPlugin()
{
    bpmTimer.stopTimer();
    notifyListenersOfDeletion();
}

void KngFilterPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    reset();
}

void KngFilterPlugin::reset()
{
    ic1 = {};
    ic2 = {};
}

float KngFilterPlugin::responseAt (float hz, float cutoffHz) const
{
    const double w = hz / juce::jmax (20.0f, cutoffHz);
    const double q = 1.0 / (2.0 - 1.96 * get (reso));
    const double den = std::sqrt ((1.0 - w * w) * (1.0 - w * w) + (w / q) * (w / q));
    const int m = juce::roundToInt (get (mode));
    const double mag = m == 0 ? 1.0 / den : m == 1 ? (w * w) / den : (w / q) / den;
    return (float) (20.0 * std::log10 (juce::jmax (1.0e-6, mag)));
}

void KngFilterPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    const int m = juce::roundToInt (get (mode));
    const float baseCutoff = get (cutoff);
    const float k = 2.0f - 1.96f * get (reso);   // lower k = more resonance
    const float driveAmount = get (drive);
    const float driveGain = 1.0f + driveAmount * 6.0f;
    const float mixAmount = get (mix);
    const float beats = wobbleBeats (juce::roundToInt (get (lfoRate)));
    const float depth = get (lfoDepth);
    const double lfoStep = beats > 0.0f ? (bpm.load() / 60.0) / beats / sampleRate : 0.0;
    const double nyquistSafe = sampleRate * 0.45;

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    float inMax = 0.0f, outMax = 0.0f;
    float g = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        // The wobble swings the cutoff up to 3 octaves either way; coefficients update every 16 samples
        if ((i & 15) == 0)
        {
            float hz = baseCutoff;

            if (beats > 0.0f)
                hz *= std::pow (2.0f, depth * 3.0f * (float) std::sin (lfoPhase * juce::MathConstants<double>::twoPi));

            hz = (float) juce::jlimit (20.0, nyquistSafe, (double) hz);
            liveCutoff.store (hz, std::memory_order_relaxed);

            g = (float) std::tan (juce::MathConstants<double>::pi * hz / sampleRate);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        lfoPhase += lfoStep;

        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;

        for (int c = 0; c < chans; ++c)
        {
            auto* d = buffer.getWritePointer (c, fc.bufferStartSample);
            const float dry = d[i];
            const float x = driveAmount > 0.0f ? std::tanh (dry * driveGain) / std::sqrt (driveGain) : dry;

            // State variable filter (one pass gives low, band and high together)
            auto& s1 = ic1[(size_t) c];
            auto& s2 = ic2[(size_t) c];
            const float v3 = x - s2;
            const float v1 = a1 * s1 + a2 * v3;
            const float v2 = s2 + a2 * s1 + a3 * v3;
            s1 = 2.0f * v1 - s1;
            s2 = 2.0f * v2 - s2;

            const float low = v2, band = v1, high = x - k * v1 - v2;
            const float wet = m == 0 ? low : m == 1 ? high : band;
            const float y = dry * (1.0f - mixAmount) + wet * mixAmount;
            d[i] = y;

            inMax = juce::jmax (inMax, std::abs (dry));
            outMax = juce::jmax (outMax, std::abs (y));
        }
    }

    reportMeters (inMax, outMax, 0.0f);
}

std::unique_ptr<te::Plugin::EditorComponent> KngFilterPlugin::createEditor()
{
    return std::make_unique<KngFilterView> (*this);
}
