#include "KngSatPlugin.h"
#include "KngSatView.h"

namespace te = tracktion;

const char* KngSatPlugin::xmlTypeName = "kngsat";

static void raisePeak (std::atomic<float>& a, float v)
{
    float current = a.load();

    while (v > current && ! a.compare_exchange_weak (current, v)) {}
}

KngSatPlugin::KngSatPlugin (te::PluginCreationInfo creationInfo)
    : te::Plugin (creationInfo)
{
    auto* um = getUndoManager();

    for (int i = 0; i < KngSat::numParams; ++i)
    {
        const auto& p = KngSat::info (i);
        juce::NormalisableRange<float> range (p.min, p.max, p.step);

        if (p.mid > p.min && p.mid < p.max)
            range.setSkewForCentre (p.mid);

        auto* value = values.add (new juce::CachedValue<float>());
        value->referTo (state, p.id, um, p.def);

        auto* param = addParam (p.id, p.name, range);
        param->attachToCurrentValue (*value);
        params[(size_t) i] = param;
    }

    // 4x oversampling with clean filters and a whole-sample delay we can line the dry signal up with
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                                                                    true, true);
    oversampler->initProcessing (chunk);
    latencySamples = juce::roundToInt (oversampler->getLatencyInSamples());

    for (auto& d : dryDelay)
        d.assign ((size_t) latencySamples + 1, 0.0f);
}

KngSatPlugin::~KngSatPlugin()
{
    notifyListenersOfDeletion();

    for (auto& p : params)
        if (p != nullptr)
            p->detachFromCurrentValue();
}

std::unique_ptr<te::Plugin::EditorComponent> KngSatPlugin::createEditor()
{
    return std::make_unique<KngSatView> (*this);
}

void KngSatPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    lastRate = 0.0;
    reset();
}

void KngSatPlugin::reset()
{
    oversampler->reset();

    for (auto& d : dryDelay)
        std::fill (d.begin(), d.end(), 0.0f);

    dryPos = 0;
    dcX = {};
    dcY = {};
    inPower = outPower = 0.0f;
    matchGain = 1.0f;

    for (auto& s : lowShelfState)  s.reset();
    for (auto& s : highShelfState) s.reset();
}

void KngSatPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    for (int i = 0; i < KngSat::numParams; ++i)
    {
        const juce::Identifier id (KngSat::info (i).id);

        if (v.hasProperty (id))
            values[i]->setValue ((float) v.getProperty (id), nullptr);
    }

    for (auto* p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

void KngSatPlugin::applyPreset (int presetIndex)
{
    const auto& list = KngSat::presets();

    if (! juce::isPositiveAndBelow (presetIndex, (int) list.size()))
        return;

    for (int i = 0; i < KngSat::numParams; ++i)
        params[(size_t) i]->setParameter (KngSat::info (i).def, juce::sendNotification);

    for (auto& [p, value] : list[(size_t) presetIndex].values)
        params[(size_t) p]->setParameter (value, juce::sendNotification);
}

void KngSatPlugin::takeMeters (float& inputPeak, float& outputPeak)
{
    inputPeak  = inPeak.exchange (0.0f);
    outputPeak = outPeak.exchange (0.0f);
}

void KngSatPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    using namespace KngSat;

    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    float v[numParams];

    for (int i = 0; i < numParams; ++i)
        v[i] = params[(size_t) i]->getCurrentValue();

    // Tone: a tilt around 1 kHz (down on one side, up on the other)
    if (v[tone] != lastTone || sampleRate != lastRate)
    {
        lowShelfCoefs  = KngEq::makeBiquad (KngEq::lowShelf,   800.0, -6.0 * v[tone], 0.707, sampleRate);
        highShelfCoefs = KngEq::makeBiquad (KngEq::highShelf, 1500.0,  6.0 * v[tone], 0.707, sampleRate);
        lastTone = v[tone];
        lastRate = sampleRate;
    }

    const int m = juce::roundToInt (v[mode]);
    const float driveGain = juce::Decibels::decibelsToGain (v[drive]);
    const float outGain = juce::Decibels::decibelsToGain (v[output]);
    const float mixAmount = v[mix];
    const bool matching = v[autoGain] > 0.5f;

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    const int delaySize = (int) dryDelay[0].size();

    float inMax = 0.0f, outMax = 0.0f;

    for (int offset = 0; offset < fc.bufferNumSamples; offset += chunk)
    {
        const int len = juce::jmin (chunk, fc.bufferNumSamples - offset);
        const int start = fc.bufferStartSample + offset;

        // Keep a delayed dry copy for the Mix knob, and measure the input
        float inSum = 0.0f;

        for (int i = 0; i < len; ++i)
        {
            for (int ch = 0; ch < chans; ++ch)
            {
                const float x = buffer.getSample (ch, start + i);
                auto& d = dryDelay[(size_t) ch];
                dryHold[(size_t) ch][(size_t) i] = d[(size_t) dryPos];
                d[(size_t) dryPos] = x;
                inSum += x * x;
                inMax = juce::jmax (inMax, std::abs (x));
            }

            if (++dryPos >= delaySize)
                dryPos = 0;
        }

        // Saturate at 4x the sample rate
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) chans, (size_t) start, (size_t) len);
        auto up = oversampler->processSamplesUp (block);

        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* d = up.getChannelPointer (ch);

            for (size_t i = 0; i < up.getNumSamples(); ++i)
                d[i] = shape (m, d[i] * driveGain);
        }

        oversampler->processSamplesDown (block);

        // Remove any DC offset, apply the tone, and measure the wet level
        float outSum = 0.0f;

        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = buffer.getWritePointer (ch, start);

            for (int i = 0; i < len; ++i)
            {
                const float x = d[i];
                const float y = x - dcX[(size_t) ch] + 0.995f * dcY[(size_t) ch];
                dcX[(size_t) ch] = x;
                dcY[(size_t) ch] = y;

                d[i] = highShelfState[(size_t) ch].process (lowShelfState[(size_t) ch].process (y, lowShelfCoefs), highShelfCoefs);
                outSum += d[i] * d[i];
            }
        }

        // Auto Gain: match the wet loudness to the dry loudness, smoothly
        const float previousGain = matchGain;

        if (matching)
        {
            inPower  = inPower * 0.9f + 0.1f * inSum / (float) (len * chans);
            outPower = outPower * 0.9f + 0.1f * outSum / (float) (len * chans);
            const float target = juce::jlimit (0.1f, 4.0f, std::sqrt ((inPower + 1.0e-9f) / (outPower + 1.0e-9f)));
            matchGain += (target - matchGain) * 0.3f;
        }
        else
        {
            matchGain = 1.0f;
        }

        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = buffer.getWritePointer (ch, start);

            for (int i = 0; i < len; ++i)
            {
                const float g = previousGain + (matchGain - previousGain) * (float) i / (float) len;
                const float wet = d[i] * g;
                const float out = (dryHold[(size_t) ch][(size_t) i] * (1.0f - mixAmount) + wet * mixAmount) * outGain;
                d[i] = out;
                outMax = juce::jmax (outMax, std::abs (out));
            }
        }
    }

    raisePeak (inPeak, inMax);
    raisePeak (outPeak, outMax);
}
