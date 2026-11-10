#include "KngVerbPlugin.h"
#include "KngVerbView.h"

namespace te = tracktion;

const char* KngVerbPlugin::xmlTypeName = "kngverb";

// Delay lengths in ms, chosen so no two lines line up and ring together
static const double lineMs[8]    = { 29.7, 37.1, 41.1, 43.7, 47.9, 53.3, 59.3, 67.1 };
static const double diffuseMs[4] = { 4.77, 3.59, 12.73, 9.30 };
static const double lfoRates[8]  = { 0.31, 0.43, 0.57, 0.71, 0.83, 0.97, 1.09, 1.21 };

KngVerbPlugin::KngVerbPlugin (te::PluginCreationInfo creationInfo)
    : te::Plugin (creationInfo)
{
    auto* um = getUndoManager();

    for (int i = 0; i < KngVerb::numParams; ++i)
    {
        const auto& p = KngVerb::info (i);
        juce::NormalisableRange<float> range (p.min, p.max, p.step);

        if (p.mid > p.min && p.mid < p.max)
            range.setSkewForCentre (p.mid);

        auto* value = values.add (new juce::CachedValue<float>());
        value->referTo (state, p.id, um, p.def);

        auto* param = addParam (p.id, p.name, range);
        param->attachToCurrentValue (*value);
        params[(size_t) i] = param;
    }
}

KngVerbPlugin::~KngVerbPlugin()
{
    notifyListenersOfDeletion();

    for (auto& p : params)
        if (p != nullptr)
            p->detachFromCurrentValue();
}

std::unique_ptr<te::Plugin::EditorComponent> KngVerbPlugin::createEditor()
{
    return std::make_unique<KngVerbView> (*this);
}

void KngVerbPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;

    // Room for the biggest size plus modulation
    for (auto& line : lines)
        line.buf.assign ((size_t) (0.4 * sampleRate) + 16, 0.0f);

    for (auto& d : diffusers)
        d.buf.assign ((size_t) (0.06 * sampleRate) + 16, 0.0f);

    preBuf.assign ((size_t) (0.3 * sampleRate) + 16, 0.0f);

    lastRate = 0.0;
    reset();
}

void KngVerbPlugin::reset()
{
    for (auto& line : lines)
    {
        std::fill (line.buf.begin(), line.buf.end(), 0.0f);
        line.write = 0;
        line.damp = 0.0f;
    }

    for (auto& d : diffusers)
    {
        std::fill (d.buf.begin(), d.buf.end(), 0.0f);
        d.pos = 0;
    }

    std::fill (preBuf.begin(), preBuf.end(), 0.0f);
    preWrite = 0;
    duckEnv = 0.0f;

    for (auto& s : lowCutState)  s.reset();
    for (auto& s : highCutState) s.reset();
}

void KngVerbPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    for (int i = 0; i < KngVerb::numParams; ++i)
    {
        const juce::Identifier id (KngVerb::info (i).id);

        if (v.hasProperty (id))
            values[i]->setValue ((float) v.getProperty (id), nullptr);
    }

    for (auto* p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

void KngVerbPlugin::applyPreset (int presetIndex)
{
    const auto& list = KngVerb::presets();

    if (! juce::isPositiveAndBelow (presetIndex, (int) list.size()))
        return;

    for (int i = 0; i < KngVerb::numParams; ++i)
        params[(size_t) i]->setParameter (KngVerb::info (i).def, juce::sendNotification);

    for (auto& [p, value] : list[(size_t) presetIndex].values)
        params[(size_t) p]->setParameter (value, juce::sendNotification);
}

void KngVerbPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    using namespace KngVerb;

    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0 || preBuf.empty())
        return;

    juce::ScopedNoDenormals noDenormals;

    float v[numParams];

    for (int i = 0; i < numParams; ++i)
        v[i] = params[(size_t) i]->getCurrentValue();

    const double sr = sampleRate;

    // Wet EQ
    if (v[lowCut] != lastLowCut || v[highCut] != lastHighCut || sr != lastRate)
    {
        lowCutCoefs  = KngEq::makeBiquad (KngEq::lowCut,  v[lowCut],  0.0, 0.707, sr);
        highCutCoefs = KngEq::makeBiquad (KngEq::highCut, v[highCut], 0.0, 0.707, sr);
        lastLowCut = v[lowCut];
        lastHighCut = v[highCut];
        lastRate = sr;
    }

    // Settings for this block
    const double scale = 0.5 + v[size] * 1.5;
    const int preSamples = juce::jlimit (0, (int) preBuf.size() - 2, (int) (v[preDelay] * 0.001 * sr));
    const float allpassG = v[diffusion] * 0.75f;
    const float dampCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * v[damping] / (float) sr);
    const double modDepth = v[modulation] * 6.0 * (sr / 48000.0);
    const double modSpeed = 0.5 + v[modulation];
    const float widthAmount = v[width];
    const float duckAmount = v[duck];
    const float mixAmount = v[mix];

    double lineDelay[8];
    float lineGain[8];

    for (int k = 0; k < 8; ++k)
    {
        lineDelay[k] = lineMs[k] * scale * 0.001 * sr;
        lineGain[k] = (float) std::pow (10.0, -3.0 * (lineDelay[k] / sr) / v[decay]);   // RT60
    }

    int diffuseLen[4];

    for (int k = 0; k < 4; ++k)
        diffuseLen[k] = juce::jlimit (1, (int) diffusers[(size_t) k].buf.size() - 1, (int) (diffuseMs[k] * scale * 0.001 * sr));

    const float duckAttack  = 1.0f - std::exp (-1.0f / (0.01f * (float) sr));
    const float duckRelease = std::exp (-1.0f / (0.25f * (float) sr));

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = chans > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;
    const int preSize = (int) preBuf.size();

    float peak = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float l = left[i];
        const float r = right != nullptr ? right[i] : l;
        const float in = 0.5f * (l + r);

        // Pre-delay
        preBuf[(size_t) preWrite] = in;
        int readPre = preWrite - preSamples;

        if (readPre < 0)
            readPre += preSize;

        float x = preBuf[(size_t) readPre];

        if (++preWrite >= preSize)
            preWrite = 0;

        // Diffusion: smears the input so the tail starts smooth, not as echoes
        for (int k = 0; k < 4; ++k)
        {
            auto& d = diffusers[(size_t) k];
            const float delayed = d.buf[(size_t) d.pos];
            const float y = delayed - allpassG * x;
            d.buf[(size_t) d.pos] = x + allpassG * y;

            if (++d.pos >= diffuseLen[k])
                d.pos = 0;

            x = y;
        }

        // Read each line, gently moving its length so the tail never rings metallic
        float outs[8];
        float sum = 0.0f;

        for (int k = 0; k < 8; ++k)
        {
            auto& line = lines[(size_t) k];
            const int n = (int) line.buf.size();

            line.lfoPhase += lfoRates[k] * modSpeed / sr;

            if (line.lfoPhase >= 1.0)
                line.lfoPhase -= 1.0;

            const double delay = lineDelay[k] + modDepth * std::sin (line.lfoPhase * juce::MathConstants<double>::twoPi);
            double pos = line.write - delay;

            while (pos < 0.0)
                pos += n;

            const int i0 = (int) pos % n;
            const int i1 = (i0 + 1) % n;
            const float frac = (float) (pos - std::floor (pos));
            const float raw = line.buf[(size_t) i0] + frac * (line.buf[(size_t) i1] - line.buf[(size_t) i0]);

            // Damping: highs fade faster than lows, like a real room
            line.damp += dampCoef * (raw - line.damp);
            outs[k] = line.damp * lineGain[k];
            sum += outs[k];
        }

        // Mix every line into every other (Householder), then feed back with fresh input
        const float mixTerm = sum * (2.0f / 8.0f);

        for (int k = 0; k < 8; ++k)
        {
            auto& line = lines[(size_t) k];
            const float inject = (k % 2 == 0 ? 0.35f : -0.35f) * x;
            line.buf[(size_t) line.write] = outs[k] - mixTerm + inject;

            if (++line.write >= (int) line.buf.size())
                line.write = 0;
        }

        // Stereo out from alternating lines
        float wetL = (outs[0] + outs[2] + outs[4] + outs[6]) * 0.35f;
        float wetR = (outs[1] + outs[3] + outs[5] + outs[7]) * 0.35f;

        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * widthAmount;
        wetL = mid + side;
        wetR = mid - side;

        wetL = highCutState[0].process (lowCutState[0].process (wetL, lowCutCoefs), highCutCoefs);
        wetR = highCutState[1].process (lowCutState[1].process (wetR, lowCutCoefs), highCutCoefs);

        // Duck: the reverb steps back while the dry signal is loud
        const float level = std::abs (in);
        duckEnv = level > duckEnv ? duckEnv + (level - duckEnv) * duckAttack : duckEnv * duckRelease;
        const float duckGain = 1.0f - duckAmount * juce::jmin (1.0f, duckEnv * 3.0f);

        wetL *= duckGain;
        wetR *= duckGain;

        left[i] = l * (1.0f - mixAmount) + wetL * mixAmount;

        if (right != nullptr)
            right[i] = r * (1.0f - mixAmount) + wetR * mixAmount;

        peak = juce::jmax (peak, std::abs (wetL), std::abs (wetR));
    }

    float current = wetPeak.load();

    while (peak > current && ! wetPeak.compare_exchange_weak (current, peak)) {}
}
