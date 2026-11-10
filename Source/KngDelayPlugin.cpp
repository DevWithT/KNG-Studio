#include "KngDelayPlugin.h"
#include "KngDelayView.h"

namespace te = tracktion;

const char* KngDelayPlugin::xmlTypeName = "kngdelay";

static float readDelayed (const std::vector<float>& buf, int writePos, double delaySamples)
{
    const int n = (int) buf.size();
    double pos = writePos - delaySamples;

    while (pos < 0.0)
        pos += n;

    const int i0 = (int) pos % n;
    const int i1 = (i0 + 1) % n;
    const float frac = (float) (pos - std::floor (pos));
    return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
}

KngDelayPlugin::KngDelayPlugin (te::PluginCreationInfo creationInfo)
    : te::Plugin (creationInfo)
{
    auto* um = getUndoManager();

    for (int i = 0; i < KngDelay::numParams; ++i)
    {
        const auto& p = KngDelay::info (i);
        juce::NormalisableRange<float> range (p.min, p.max, p.step);

        if (p.mid > p.min && p.mid < p.max)
            range.setSkewForCentre (p.mid);

        auto* value = values.add (new juce::CachedValue<float>());
        value->referTo (state, p.id, um, p.def);

        auto* param = addParam (p.id, p.name, range);
        param->attachToCurrentValue (*value);
        params[(size_t) i] = param;
    }

    // Follow the song's tempo from the message thread
    timerCallback();
    startTimerHz (4);
}

KngDelayPlugin::~KngDelayPlugin()
{
    stopTimer();
    notifyListenersOfDeletion();

    for (auto& p : params)
        if (p != nullptr)
            p->detachFromCurrentValue();
}

std::unique_ptr<te::Plugin::EditorComponent> KngDelayPlugin::createEditor()
{
    return std::make_unique<KngDelayView> (*this);
}

void KngDelayPlugin::timerCallback()
{
    bpm.store (edit.tempoSequence.getBpmAt (edit.getTransport().getPosition()));
}

double KngDelayPlugin::getDelaySeconds (int side) const
{
    float v[KngDelay::numParams];

    for (int i = 0; i < KngDelay::numParams; ++i)
        v[i] = params[(size_t) i]->getCurrentValue();

    return KngDelay::delaySeconds (v, side, bpm.load());
}

void KngDelayPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;

    // Up to 4 seconds of delay, plus room for wobble
    bufL.assign ((size_t) (4.5 * sampleRate), 0.0f);
    bufR.assign ((size_t) (4.5 * sampleRate), 0.0f);

    lastRate = 0.0;
    reset();
}

void KngDelayPlugin::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writePos = 0;
    smoothL = smoothR = 0.0;
    duckEnv = 0.0f;

    for (auto& s : lowCutState)  s.reset();
    for (auto& s : highCutState) s.reset();
}

void KngDelayPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    for (int i = 0; i < KngDelay::numParams; ++i)
    {
        const juce::Identifier id (KngDelay::info (i).id);

        if (v.hasProperty (id))
            values[i]->setValue ((float) v.getProperty (id), nullptr);
    }

    for (auto* p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

void KngDelayPlugin::applyPreset (int presetIndex)
{
    const auto& list = KngDelay::presets();

    if (! juce::isPositiveAndBelow (presetIndex, (int) list.size()))
        return;

    for (int i = 0; i < KngDelay::numParams; ++i)
        params[(size_t) i]->setParameter (KngDelay::info (i).def, juce::sendNotification);

    for (auto& [p, value] : list[(size_t) presetIndex].values)
        params[(size_t) p]->setParameter (value, juce::sendNotification);
}

void KngDelayPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    using namespace KngDelay;

    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0 || bufL.empty())
        return;

    juce::ScopedNoDenormals noDenormals;

    float v[numParams];

    for (int i = 0; i < numParams; ++i)
        v[i] = params[(size_t) i]->getCurrentValue();

    const double sr = sampleRate;
    const int n = (int) bufL.size();

    if (v[lowCut] != lastLowCut || v[highCut] != lastHighCut || sr != lastRate)
    {
        lowCutCoefs  = KngEq::makeBiquad (KngEq::lowCut,  v[lowCut],  0.0, 0.707, sr);
        highCutCoefs = KngEq::makeBiquad (KngEq::highCut, v[highCut], 0.0, 0.707, sr);
        lastLowCut = v[lowCut];
        lastHighCut = v[highCut];
        lastRate = sr;
    }

    const int m = juce::roundToInt (v[mode]);
    const double maxDelay = n - 8.0;
    const double targetL = juce::jlimit (1.0, maxDelay, delaySeconds (v, 0, bpm.load()) * sr);
    const double targetR = m == mono ? targetL : juce::jlimit (1.0, maxDelay, delaySeconds (v, 1, bpm.load()) * sr);

    if (smoothL <= 0.0) smoothL = targetL;
    if (smoothR <= 0.0) smoothR = targetR;

    // Time changes glide like tape instead of clicking
    const double glide = 1.0 - std::exp (-1.0 / (0.05 * sr));
    const float fb = v[feedback];
    const float driveGain = 1.0f + v[drive] * 4.0f;
    const double wobbleDepth = v[wobble] * 0.004 * sr;
    const float widthAmount = v[width], duckAmount = v[duck], mixAmount = v[mix];
    const float duckAttack  = 1.0f - std::exp (-1.0f / (0.01f * (float) sr));
    const float duckRelease = std::exp (-1.0f / (0.25f * (float) sr));
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = chans > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;

    float peak = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float l = left[i];
        const float r = right != nullptr ? right[i] : l;
        const float monoIn = 0.5f * (l + r);

        smoothL += (targetL - smoothL) * glide;
        smoothR += (targetR - smoothR) * glide;

        // Wow (slow) and flutter (fast), like a worn tape machine
        wowPhase += 0.6 / sr;
        flutterPhase += 7.0 / sr;

        if (wowPhase >= 1.0)     wowPhase -= 1.0;
        if (flutterPhase >= 1.0) flutterPhase -= 1.0;

        const double wob = wobbleDepth * (std::sin (wowPhase * twoPi) + 0.15 * std::sin (flutterPhase * twoPi));

        const float rawL = readDelayed (bufL, writePos, smoothL + wob);
        const float rawR = readDelayed (bufR, writePos, smoothR + wob * 0.8);

        // Filter the repeats, then warm them up
        float wetL = highCutState[0].process (lowCutState[0].process (rawL, lowCutCoefs), highCutCoefs);
        float wetR = highCutState[1].process (lowCutState[1].process (rawR, lowCutCoefs), highCutCoefs);
        wetL = std::tanh (wetL * driveGain) / driveGain;
        wetR = std::tanh (wetR * driveGain) / driveGain;

        if (m == pingPong)
        {
            // Input goes in on the left, then each repeat swaps sides
            bufL[(size_t) writePos] = monoIn + wetR * fb;
            bufR[(size_t) writePos] = wetL * fb;
        }
        else if (m == mono)
        {
            const float in = monoIn + wetL * fb;
            bufL[(size_t) writePos] = in;
            bufR[(size_t) writePos] = in;
            wetR = wetL;
        }
        else
        {
            bufL[(size_t) writePos] = l + wetL * fb;
            bufR[(size_t) writePos] = r + wetR * fb;
        }

        if (++writePos >= n)
            writePos = 0;

        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * widthAmount;
        float outL = mid + side;
        float outR = mid - side;

        // Duck: repeats step back while the dry signal is loud
        const float level = std::abs (monoIn);
        duckEnv = level > duckEnv ? duckEnv + (level - duckEnv) * duckAttack : duckEnv * duckRelease;
        const float duckGain = 1.0f - duckAmount * juce::jmin (1.0f, duckEnv * 3.0f);
        outL *= duckGain;
        outR *= duckGain;

        left[i] = l * (1.0f - mixAmount) + outL * mixAmount;

        if (right != nullptr)
            right[i] = r * (1.0f - mixAmount) + outR * mixAmount;

        peak = juce::jmax (peak, std::abs (outL), std::abs (outR));
    }

    float current = wetPeak.load();

    while (peak > current && ! wetPeak.compare_exchange_weak (current, peak)) {}
}
