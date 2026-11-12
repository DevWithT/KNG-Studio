#include "KngDoublerPlugin.h"
#include "KngDoublerView.h"

namespace te = tracktion;

const char* KngDoublerPlugin::xmlTypeName = "kngdoubler";

// Each voice: delay multiplier, drift speed (Hz), stereo position, start phase, detune direction
struct DoublerVoice { float mul; double rate; float pan; double phase; float sign; };

static const DoublerVoice voiceSetup[4] = {
    { 1.00f, 0.43, -1.0f, 0.00,  1.0f },
    { 1.31f, 0.57,  1.0f, 0.50, -1.0f },
    { 0.72f, 0.37, -0.6f, 0.25, -1.0f },
    { 1.58f, 0.71,  0.6f, 0.75,  1.0f }
};

static std::vector<KngParamDef> doublerParams()
{
    return {
        { "detune", "Detune",   0.0f,   30.0f,  10.0f,   0.0f, 0.0f, "ct", nullptr },
        { "delay",  "Delay",    5.0f,   40.0f,  18.0f,   0.0f, 0.0f, "ms", nullptr },
        { "voices", "Voices",   0.0f,    1.0f,   0.0f,   0.0f, 1.0f, "",   "2 Voices|4 Voices" },
        { "width",  "Width",    0.0f,    1.0f,   1.0f,   0.0f, 0.0f, "%",  nullptr },
        { "lowCut", "Low Cut", 20.0f, 1000.0f, 250.0f, 150.0f, 0.0f, "Hz", nullptr },
        { "mix",    "Mix",      0.0f,    1.0f,   0.5f,   0.0f, 0.0f, "%",  nullptr }
    };
}

static std::vector<KngPresetDef> doublerPresets()
{
    using P = KngDoublerPlugin;

    return {
        { "Default", {} },
        { "Subtle Double",  { { P::detune, 6.0f },  { P::delay, 15.0f }, { P::mix, 0.35f } } },
        { "Wide Ad-libs",   { { P::detune, 14.0f }, { P::delay, 25.0f }, { P::voices, 1.0f }, { P::mix, 0.6f } } },
        { "Hook Thickener", { { P::detune, 10.0f }, { P::delay, 20.0f }, { P::voices, 1.0f }, { P::mix, 0.5f } } },
        { "Robot Wide",     { { P::detune, 25.0f }, { P::delay, 8.0f },  { P::mix, 0.5f } } }
    };
}

KngDoublerPlugin::KngDoublerPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, doublerParams(), doublerPresets())
{
}

void KngDoublerPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    buf.assign ((size_t) (0.2 * sampleRate), 0.0f);
    lastRate = 0.0;
    reset();
}

void KngDoublerPlugin::reset()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    writePos = 0;

    for (int v = 0; v < 4; ++v)
        phases[(size_t) v] = voiceSetup[v].phase;

    for (auto& s : lowCutState)
        s.reset();
}

void KngDoublerPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0 || buf.empty())
        return;

    juce::ScopedNoDenormals noDenormals;

    if (get (lowCut) != lastLowCut || sampleRate != lastRate)
    {
        lowCutCoefs = KngEq::makeBiquad (KngEq::lowCut, get (lowCut), 0.0, 0.707, sampleRate);
        lastLowCut = get (lowCut);
        lastRate = sampleRate;
    }

    const int numVoices = get (voices) > 0.5f ? 4 : 2;
    const double sr = sampleRate;
    const double baseDelay = get (delay) * 0.001 * sr;
    const double centsRatio = std::pow (2.0, get (detune) / 1200.0) - 1.0;
    const float widthAmount = get (width), mixAmount = get (mix);
    const float norm = 1.0f / std::sqrt ((float) numVoices * 0.5f);
    const int n = (int) buf.size();
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    // How far each voice's delay swings to drift by the chosen number of cents
    double depth[4];

    for (int v = 0; v < 4; ++v)
        depth[v] = centsRatio * sr / (twoPi * voiceSetup[v].rate);

    auto& buffer = *fc.destBuffer;
    const int chans = juce::jmin (2, buffer.getNumChannels());
    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = chans > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;

    float inMax = 0.0f, outMax = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float l = left[i];
        const float r = right != nullptr ? right[i] : l;
        buf[(size_t) writePos] = 0.5f * (l + r);

        float wetL = 0.0f, wetR = 0.0f;

        for (int v = 0; v < numVoices; ++v)
        {
            const auto& setup = voiceSetup[v];
            auto& ph = phases[(size_t) v];
            ph += setup.rate / sr;

            if (ph >= 1.0)
                ph -= 1.0;

            const double d = baseDelay * setup.mul + depth[v] * (1.0 + setup.sign * std::sin (ph * twoPi));
            double p = writePos - juce::jlimit (1.0, (double) n - 2.0, d);

            while (p < 0.0)
                p += n;

            const int i0 = (int) p % n;
            const int i1 = (i0 + 1) % n;
            const float frac = (float) (p - std::floor (p));
            const float raw = buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
            const float s = lowCutState[(size_t) v].process (raw, lowCutCoefs);

            const float pan = setup.pan * widthAmount;
            wetL += s * std::sqrt (0.5f * (1.0f - pan));
            wetR += s * std::sqrt (0.5f * (1.0f + pan));
        }

        if (++writePos >= n)
            writePos = 0;

        // The lead stays centred; the doubles sit around it
        const float ol = l * (1.0f - mixAmount * 0.3f) + wetL * norm * mixAmount;
        const float orr = r * (1.0f - mixAmount * 0.3f) + wetR * norm * mixAmount;

        left[i] = ol;

        if (right != nullptr)
            right[i] = orr;

        inMax = juce::jmax (inMax, std::abs (l), std::abs (r));
        outMax = juce::jmax (outMax, std::abs (ol), std::abs (orr));
    }

    reportMeters (inMax, outMax, 0.0f);
}

std::unique_ptr<te::Plugin::EditorComponent> KngDoublerPlugin::createEditor()
{
    return std::make_unique<KngDoublerView> (*this);
}

void KngDoublerPlugin::drawDisplay (juce::Graphics& g, juce::Rectangle<float> area, const KngHistory&)
{
    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (area, 6.0f);

    const float cx = area.getCentreX(), cy = area.getCentreY();
    const float halfW = area.getWidth() * 0.4f;
    const int numVoices = get (voices) > 0.5f ? 4 : 2;

    // The stereo field, left to right
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine ((int) cy, cx - halfW, cx + halfW);
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("L", (int) (cx - halfW) - 16, (int) cy - 8, 12, 16, juce::Justification::centred);
    g.drawText ("R", (int) (cx + halfW) + 4, (int) cy - 8, 12, 16, juce::Justification::centred);

    // Your vocal in the middle, the doubles around it
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ cx, cy }));

    const float size = 10.0f + get (mix) * 10.0f;

    for (int v = 0; v < numVoices; ++v)
    {
        const float x = cx + voiceSetup[v].pan * get (width) * halfW;
        const float y = cy + (v % 2 == 0 ? -18.0f : 18.0f);
        g.setColour (juce::Colour (0xff3a8ee6).withAlpha (0.85f));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ x, y }));
    }

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (juce::String (numVoices) + " voices    " + juce::String (juce::roundToInt (get (detune))) + " ct    "
                    + juce::String (juce::roundToInt (get (delay))) + " ms",
                area.reduced (12.0f).toNearestInt(), juce::Justification::topLeft);
}
