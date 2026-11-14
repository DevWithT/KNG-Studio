#include "KngPumpPlugin.h"
#include "KngPumpView.h"

namespace te = tracktion;

const char* KngPumpPlugin::xmlTypeName = "kngpump";

static std::vector<KngParamDef> pumpParams()
{
    return {
        { "rate",     "Rate",     0.0f, 4.0f, 2.0f, 0.0f, 1.0f, "",  "1 Bar|1/2|1/4|1/8|1/16" },
        { "depth",    "Depth",    0.0f, 1.0f, 0.8f, 0.0f, 0.0f, "%", nullptr },
        { "recovery", "Recovery", 0.1f, 1.0f, 0.6f, 0.0f, 0.0f, "%", nullptr },
        { "curve",    "Curve",    0.0f, 1.0f, 0.4f, 0.0f, 0.0f, "%", nullptr },
        { "offset",   "Offset",   0.0f, 0.5f, 0.0f, 0.0f, 0.0f, "%", nullptr }
    };
}

static std::vector<KngPresetDef> pumpPresets()
{
    using P = KngPumpPlugin;

    return {
        { "Default", {} },
        { "Classic Pump",  { { P::rate, 2.0f }, { P::depth, 0.85f }, { P::recovery, 0.6f }, { P::curve, 0.4f } } },
        { "Subtle Breath", { { P::rate, 2.0f }, { P::depth, 0.4f }, { P::recovery, 0.8f }, { P::curve, 0.2f } } },
        { "Hard Duck",     { { P::rate, 2.0f }, { P::depth, 1.0f }, { P::recovery, 0.4f }, { P::curve, 0.8f } } },
        { "Eighth Chop",   { { P::rate, 3.0f }, { P::depth, 0.9f }, { P::recovery, 0.5f }, { P::curve, 0.7f } } },
        { "Half-Time Swell", { { P::rate, 1.0f }, { P::depth, 0.7f }, { P::recovery, 0.9f }, { P::curve, 0.1f } } }
    };
}

static double cycleBeats (int r)
{
    static const double beats[] = { 4.0, 2.0, 1.0, 0.5, 0.25 };
    return beats[juce::jlimit (0, 4, r)];
}

KngPumpPlugin::KngPumpPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, pumpParams(), pumpPresets())
{
    bpmTimer.onTick = [this]
    {
        bpm = (float) edit.tempoSequence.getBpmAt (edit.getTransport().getPosition());
    };

    bpmTimer.onTick();
    bpmTimer.startTimerHz (4);
}

KngPumpPlugin::~KngPumpPlugin()
{
    bpmTimer.stopTimer();
    notifyListenersOfDeletion();
}

void KngPumpPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    reset();
}

void KngPumpPlugin::reset()
{
    freePhase = 0.0;
    smoothGain = 1.0f;
}

float KngPumpPlugin::shapeAt (float cyclePos) const
{
    // Ducks at the start of each cycle, then comes back up over Recovery with the chosen Curve
    float t = cyclePos - get (offset);

    if (t < 0.0f)
        t += 1.0f;

    const float back = juce::jlimit (0.0f, 1.0f, t / juce::jmax (0.05f, get (recovery)));
    const float shaped = std::pow (back, 0.4f + get (curve) * 2.6f);
    return 1.0f - get (depth) * (1.0f - shaped);
}

void KngPumpPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    const double beatsPerSecond = juce::jlimit (40.0, 300.0, (double) bpm.load()) / 60.0;
    const double lengthBeats = cycleBeats (juce::roundToInt (get (rate)));
    const double cyclesPerSample = beatsPerSecond / lengthBeats / sampleRate;

    // While playing, lock to the song's beat; when stopped, keep pumping on its own
    double phase = freePhase;

    if (fc.isPlaying)
    {
        const double beatsNow = fc.editTime.getStart().inSeconds() * beatsPerSecond;
        phase = beatsNow / lengthBeats;
        phase -= std::floor (phase);
    }

    const float smooth = 1.0f - std::exp (-1.0f / (0.001f * (float) sampleRate));   // 1 ms, so the duck doesn't click

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    float inMax = 0.0f, outMax = 0.0f, lowest = 1.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float target = shapeAt ((float) phase);
        smoothGain += (target - smoothGain) * smooth;
        lowest = juce::jmin (lowest, smoothGain);

        for (int c = 0; c < chans; ++c)
        {
            auto* d = buffer.getWritePointer (c, fc.bufferStartSample);
            const float x = d[i];
            d[i] = x * smoothGain;
            inMax = juce::jmax (inMax, std::abs (x));
            outMax = juce::jmax (outMax, std::abs (d[i]));
        }

        phase += cyclesPerSample;

        if (phase >= 1.0)
            phase -= 1.0;
    }

    freePhase = phase;
    cycleNow = (float) phase;
    reportMeters (inMax, outMax, -juce::Decibels::gainToDecibels (juce::jmax (0.001f, lowest)));
}

std::unique_ptr<te::Plugin::EditorComponent> KngPumpPlugin::createEditor()
{
    return std::make_unique<KngPumpView> (*this);
}
